#!/usr/bin/env python3
"""
L3-клиент для ESP32 по UART или BLE: подключение по beacon (accept), ответы на пинги, отправка данных
в форматах RAW / SIGN / ENCRYPT и проверочные «атаки» (повтор, подделка, чужой ключ...).

Зависимости:  pip install pyserial cryptography        (UART)
              pip install bleak cryptography           (BLE)

Примеры:
    python l3_client.py --port COM5 --key 00112233...eeff
    python l3_client.py --port /dev/ttyUSB0 --key-file key.hex --format encrypt
    python l3_client.py --port COM5 --monitor            # только слушать, без ключа тоже можно
    python l3_client.py --ble --key-file key.hex         # BLE: найти устройство по рекламе, формат ENCRYPT
    python l3_client.py --ble --sn 0102030405060708 --key-file key.hex

BLE: устройство ищется по рекламе (Company ID 0xFFFF + "PX" + SN), сервис Nordic UART. Одно уведомление
или одна запись — один кадр. Соединение рвётся (ESP: нет сеанса 10 с, пинги без ответа) — клиент
подключается заново сам.

Команды в консоли (help — список):
    10 01 02 ff            отправить сообщение (hex, первый байт — fId), длина перед ним добавится сама
    10 01 | 20 02 03       несколько сообщений в одном кадре
    /fmt raw|sign|encrypt  сменить формат отправки
    /replay  /tamper  /stale  /badkey  /badinst  /dir  /crc  /oldboot   проверочные кадры (ESP должен отклонить)
    /quiet                 перестать отвечать на пинги (ESP закроет сеанс и сменит его идентификатор)
    /flood N               отправить N пакетов подряд
    /resync  /status  /quit

ESP на отклонённые пакеты ничего не отвечает. Видимые признаки:
    - идентификатор сеанса (IV[8..11] beacon): пока ESP держит наш сеанс, он не меняется;
      сменился — ESP закрыл сеанс или перезапустился, клиент подключается заново;
    - пинги от ESP: идут, только если сеанс открыт и от нас нет пакетов;
    - логи ESP (если они идут в тот же UART, клиент печатает их строками [esp]).
"""

import argparse
import os
import queue
import struct
import sys
import threading
import time

try:
    import serial
except ImportError:
    serial = None               # нужен только для UART: проверка в main()
try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
except ImportError:
    sys.exit("Нужен cryptography:  pip install pyserial cryptography")


# ---------------------------------------------------------------- протокол

HEAD = b"PX"
VERSION = 0x02
HEADER_LEN = 6
IV_LEN = 12
TAG_LEN = 16
DATA_MAX = 128                                  # PACKET_DATA_LEN_MAX; канал может урезать (--data-max)
PAYLOAD_MAX = IV_LEN + DATA_MAX + TAG_LEN      # 156
CRC_LEN = 2

DIR_FROM = 0    # от ESP
DIR_TO = 1      # к ESP

FMT_RAW, FMT_SIGN, FMT_ENCRYPT = 0, 1, 3
FMT_NAMES = {FMT_RAW: "RAW", FMT_SIGN: "SIGN", FMT_ENCRYPT: "ENCRYPT"}
FMT_BY_NAME = {"raw": FMT_RAW, "sign": FMT_SIGN, "encrypt": FMT_ENCRYPT}

N_MAX = (1 << 56) - 1

FID_BEACON = 0x00
FID_ACCEPT = 0x01        # клиент -> ESP после beacon: открыть сеанс; ESP -> клиент: сеанс открыт
FID_PING = 0x05
BEACON_HEAD = struct.Struct("<BH8sB")          # fId, interval, sn, length = 12 байт, дальше length байт имени
BEACON_NAME_MAX = 32


def crc16_xmodem(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def make_iv(n: int, instance: int, rnd: bytes) -> bytes:
    epoch = (n >> 32) & 0xFFFFFF
    return epoch.to_bytes(3, "little") + bytes([instance & 0xFF]) + (n & 0xFFFFFFFF).to_bytes(4, "little") + rnd


def iv_parts(iv: bytes):
    epoch = int.from_bytes(iv[0:3], "little")
    counter = int.from_bytes(iv[4:8], "little")
    return epoch, iv[3], counter, (epoch << 32) | counter, iv[8:12]


def build_frame(direction: int, fmt: int, data: bytes, key: bytes = None, iv: bytes = None, bind: bytes = b"") -> bytes:
    """Собрать кадр. Для SIGN/ENCRYPT нужны key (32 байта) и iv (12 байт).
    bind — 4 байта идентификатора сеанса ESP (IV beacon [8..11]) в конец AAD; обязателен для кадров клиента (к ESP),
    в эфир не передаётся."""
    if fmt == FMT_RAW:
        if len(data) > PAYLOAD_MAX:
            raise ValueError("RAW: данных больше %d байт" % PAYLOAD_MAX)
        payload_len = len(data)
    else:
        if len(data) > DATA_MAX:
            raise ValueError("SIGN/ENCRYPT: данных больше %d байт" % DATA_MAX)
        payload_len = IV_LEN + len(data) + TAG_LEN

    header = HEAD + bytes([(VERSION & 0x0F) | ((direction & 1) << 7), fmt & 0x07]) + struct.pack("<H", payload_len)

    if fmt == FMT_RAW:
        payload = data
    elif fmt == FMT_SIGN:
        # GMAC: всё под AAD, шифротекст пустой, encrypt() возвращает только тег
        tag = AESGCM(key).encrypt(iv, b"", header + iv + data + bind)
        payload = iv + data + tag
    elif fmt == FMT_ENCRYPT:
        ct_tag = AESGCM(key).encrypt(iv, data, header + iv + bind)
        payload = iv + ct_tag
    else:
        raise ValueError("формат %d не поддерживается" % fmt)

    frame = header + payload
    return frame + struct.pack("<H", crc16_xmodem(frame))


class Frame:
    def __init__(self, raw: bytes):
        self.raw = raw
        self.version = raw[2] & 0x0F
        self.direction = raw[2] >> 7
        self.fmt = raw[3] & 0x07
        self.payload = raw[HEADER_LEN:-CRC_LEN]
        self.header = raw[:HEADER_LEN]

    @property
    def secure(self):
        return self.fmt in (FMT_SIGN, FMT_ENCRYPT)

    @property
    def iv(self):
        return self.payload[:IV_LEN]

    def open(self, key: bytes):
        """Проверить тег и вернуть DATA. Исключение — подпись не сошлась."""
        if self.fmt == FMT_RAW:
            return self.payload
        if len(self.payload) < IV_LEN + TAG_LEN:
            raise ValueError("payload короче IV+TAG")
        iv = self.payload[:IV_LEN]
        body = self.payload[IV_LEN:-TAG_LEN]
        tag = self.payload[-TAG_LEN:]
        if self.fmt == FMT_SIGN:
            AESGCM(key).decrypt(iv, tag, self.header + iv + body)
            return body
        if self.fmt == FMT_ENCRYPT:
            return AESGCM(key).decrypt(iv, body + tag, self.header + iv)
        raise ValueError("формат %d не поддерживается" % self.fmt)


class FrameParser:
    """Поток байт -> кадры. Всё, что не кадр (например, логи ESP в том же UART), отдаётся как мусор."""

    STALL = 0.2     # с: недособранный кадр дольше этого — ложная сигнатура, сдвигаемся

    def __init__(self):
        self.buf = bytearray()
        self.since = None

    def feed(self, data: bytes):
        """Возвращает список ('frame', Frame) | ('junk', bytes) | ('crc', bytes)."""
        out = []
        self.buf += data
        while True:
            i = self.buf.find(HEAD)
            if i < 0:
                keep = 1 if self.buf.endswith(HEAD[:1]) else 0
                junk = bytes(self.buf[:len(self.buf) - keep])
                if junk:
                    out.append(("junk", junk))
                del self.buf[:len(self.buf) - keep]
                self.since = None
                break
            if i > 0:
                out.append(("junk", bytes(self.buf[:i])))
                del self.buf[:i]
            if len(self.buf) < HEADER_LEN:
                break
            plen = struct.unpack_from("<H", self.buf, 4)[0]
            if plen > PAYLOAD_MAX:
                out.append(("junk", bytes(self.buf[:1])))
                del self.buf[:1]
                continue
            total = HEADER_LEN + plen + CRC_LEN
            if len(self.buf) < total:
                now = time.monotonic()
                if self.since is None:
                    self.since = now
                elif now - self.since > self.STALL:
                    out.append(("junk", bytes(self.buf[:1])))
                    del self.buf[:1]
                    self.since = None
                    continue
                break
            raw = bytes(self.buf[:total])
            crc = struct.unpack_from("<H", raw, total - CRC_LEN)[0]
            self.since = None
            if crc != crc16_xmodem(raw[:-CRC_LEN]):
                out.append(("crc", raw))
                out.append(("junk", bytes(self.buf[:1])))
                del self.buf[:1]
                continue
            del self.buf[:total]
            out.append(("frame", Frame(raw)))
        return out


def pack(*messages: bytes) -> bytes:
    """Сообщения (fId + данные) -> DATA: перед каждым его длина, [len][fId][данные…]."""
    out = bytearray()
    for m in messages:
        if not 1 <= len(m) <= 255:
            raise ValueError("длина сообщения 1..255")
        out.append(len(m))
        out += m
    return bytes(out)


def split_messages(data: bytes):
    """DATA -> [(fId, сообщение)], сообщение = fId + данные (без байта длины).
    Неверная длина (0 или выходит за DATA) — остаток пропускается."""
    out = []
    i = 0
    while i < len(data):
        n = data[i]
        if n == 0 or i + 1 + n > len(data):
            break
        msg = data[i + 1:i + 1 + n]
        out.append((msg[0], msg))
        i += 1 + n
    return out


def parse_beacon(msg: bytes):
    """Beacon переменной длины: 12 байт заголовка + length байт имени. None — сообщение битое."""
    if len(msg) < BEACON_HEAD.size:
        return None
    fid, interval, sn, length = BEACON_HEAD.unpack_from(msg)
    if length > BEACON_NAME_MAX or len(msg) != BEACON_HEAD.size + length:
        return None
    name = msg[BEACON_HEAD.size:].decode("utf-8", "replace")
    return {"interval": interval, "sn": sn.hex(), "name": name}


def hx(b: bytes) -> str:
    return b.hex(" ")


# ---------------------------------------------------------------- транспорт BLE

NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"    # запись: клиент -> ESP
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"    # уведомления: ESP -> клиент
BLE_COMPANY_ID = 0xFFFF
BLE_MTU_MIN = HEADER_LEN + PAYLOAD_MAX + CRC_LEN + 3    # кадр 164 байт целиком в одном уведомлении: 167


class BlePort:
    """BLE с тем же интерфейсом, что у serial.Serial (read / in_waiting / write / close): Client не меняется.
    bleak работает в своём потоке с asyncio; уведомления складываются в буфер, read() их отдаёт."""

    def __init__(self, address=None, sn=None, log=print):
        import asyncio
        try:
            import bleak
        except ImportError:
            sys.exit("Нужен bleak:  pip install bleak cryptography")
        self.asyncio = asyncio
        self.bleak = bleak
        self.log = log
        self.want_address = address.upper() if address else None
        self.want_sn = sn.lower() if sn else None
        self.loop = asyncio.new_event_loop()
        threading.Thread(target=self.loop.run_forever, daemon=True).start()
        self.buf = bytearray()
        self.cv = threading.Condition()
        self.client = None
        self.address = None             # выбранное устройство: к нему же подключаемся заново
        self.connected = False
        self.closing = False
        self.reconnecting = False       # цикл переподключения уже идёт: второй не запускать
        self.disconnected = False       # для Client.reader: сбросить сеанс

    def _run(self, coro, timeout=None):
        return self.asyncio.run_coroutine_threadsafe(coro, self.loop).result(timeout)

    # --- поиск

    async def _scan(self, seconds):
        found = await self.bleak.BleakScanner.discover(timeout=seconds, return_adv=True)
        out = []
        for dev, adv in found.values():
            md = (adv.manufacturer_data or {}).get(BLE_COMPANY_ID)
            if md and md[:2] == HEAD and len(md) >= 10:
                out.append((dev, md[2:10].hex(), adv.local_name or dev.name or "", adv.rssi))
        return out

    def _match(self, item):
        dev, sn, name, rssi = item
        if self.want_address and dev.address.upper() != self.want_address:
            return False
        if self.want_sn and sn != self.want_sn:
            return False
        return True

    def find(self):
        """Найти устройство по рекламе. Несколько подходящих — спросить, какое. Возвращает BLEDevice."""
        while True:
            self.log("BLE: поиск устройств (0xFFFF + PX)...")
            try:
                items = [i for i in self._run(self._scan(4.0)) if self._match(i)]
            except Exception as e:
                # Нет адаптера, он выключен или не умеет BLE (роль central): дальше пробовать бессмысленно
                sys.exit("BLE недоступен: %s\n"
                         "Проверьте, что Bluetooth включён и адаптер поддерживает Bluetooth 4.0+ (BLE). "
                         "Подойдёт любой USB-адаптер Bluetooth 5.0." % (e or type(e).__name__))
            if items:
                break
            self.log("BLE: не найдено, ищем ещё")
        items.sort(key=lambda i: -(i[3] or -999))
        for k, (dev, sn, name, rssi) in enumerate(items):
            self.log("  %d) %s  SN %s  «%s»  RSSI %s" % (k + 1, dev.address, sn, name, rssi))
        pick = 0
        if len(items) > 1:
            while True:
                try:
                    out_flush()     # список должен быть на экране до вопроса
                    pick = int(input("номер устройства: ")) - 1
                    if 0 <= pick < len(items):
                        break
                except ValueError:
                    pass
        dev, sn, name, rssi = items[pick]
        self.address = dev.address
        self.want_address = dev.address.upper()     # переподключение — только к нему
        self.log("BLE: выбрано %s, SN %s «%s» (SN и имя из рекламы не проверены)" % (dev.address, sn, name))
        return dev

    # --- соединение

    async def _connect(self, dev):
        # Рабочим (self.client) соединение становится только целиком: подключено и уведомления включены.
        # Неудачная попытка закрывается и не подменяет рабочее соединение (иначе запись уйдёт не туда)
        # use_cached_services=False: Windows при быстром переподключении отдаёт устаревший список сервисов
        client = self.bleak.BleakClient(dev, disconnected_callback=self._on_disconnect,
                                        winrt={"use_cached_services": False})
        try:
            await client.connect(timeout=15.0)
            backend = getattr(client, "_backend", None)
            if backend is not None and hasattr(backend, "_acquire_mtu"):    # BlueZ: узнать настоящий MTU
                try:
                    await backend._acquire_mtu()
                except Exception:
                    pass
            mtu = client.mtu_size
            await client.start_notify(NUS_TX, self._on_notify)     # ESP после этого сразу шлёт beacon
        except BaseException:
            try:
                await client.disconnect()
            except Exception:
                pass
            raise
        self.client = client
        self.connected = True
        return mtu

    def connect(self, dev):
        while True:
            try:
                mtu = self._run(self._connect(dev), timeout=30)
                self.log("BLE: подключено, MTU %d%s, уведомления включены — ждём beacon"
                         % (mtu, "" if mtu >= BLE_MTU_MIN else " (меньше %d: ESP не будет слать кадры)" % BLE_MTU_MIN))
                return
            except Exception as e:
                self.log("BLE: не подключились (%s), повтор через 1 с" % (e or type(e).__name__))
                time.sleep(1.0)

    def _on_notify(self, sender, data):
        with self.cv:
            self.buf += data
            self.cv.notify()

    def _on_disconnect(self, client):
        if client is not self.client or not self.connected:
            return                      # неудачная попытка или уже обработано
        self.connected = False
        self.disconnected = True
        if self.closing or self.reconnecting:
            return
        self.log("BLE: соединение разорвано — подключаемся заново")
        self.reconnecting = True
        self.loop.create_task(self._reconnect())

    async def _reconnect(self):
        try:
            await self.asyncio.sleep(1.0)
            while not self.closing:
                items = [i for i in await self._scan(4.0) if self._match(i)]
                if not items:
                    self.log("BLE: устройство не видно (может быть занято другим телефоном), ищем ещё")
                    continue
                try:
                    mtu = await self._connect(items[0][0])
                    self.log("BLE: подключено заново, MTU %d — ждём beacon" % mtu)
                    return
                except Exception as e:
                    self.log("BLE: не подключились (%s), повтор через 2 с" % (e or type(e).__name__))
                    await self.asyncio.sleep(2.0)
        finally:
            self.reconnecting = False

    def take_disconnect(self):
        if self.disconnected:
            self.disconnected = False
            with self.cv:
                self.buf.clear()
            return True
        return False

    # --- интерфейс serial.Serial

    @property
    def in_waiting(self):
        return len(self.buf)

    def read(self, n=1):
        with self.cv:
            if not self.buf:
                self.cv.wait(0.02)
            out = bytes(self.buf[:n])
            del self.buf[:n]
            return out

    def write(self, data):
        if not self.connected:
            self.log("BLE: нет соединения — кадр не отправлен")
            return
        try:
            self._run(self.client.write_gatt_char(NUS_RX, bytes(data), response=False), timeout=5)
        except Exception as e:
            self.log("BLE: запись не удалась: %s" % (e or type(e).__name__))

    def close(self):
        self.closing = True
        if self.client is not None and self.connected:
            try:
                self._run(self.client.disconnect(), timeout=5)
            except Exception:
                pass
        self.loop.call_soon_threadsafe(self.loop.stop)


# ---------------------------------------------------------------- вывод

# Печать — в отдельном потоке через очередь. Если консоль «заморожена» (в Windows — выделение мышью),
# print блокируется; без очереди вместе с ним встали бы приём кадров и ответы на пинги, и ESP закрыл бы сеанс
_out_q = queue.Queue()


def _out_worker():
    while True:
        line = _out_q.get()
        print(line, flush=True)


threading.Thread(target=_out_worker, daemon=True).start()


def out(text):
    t = time.strftime("%H:%M:%S") + ".%03d" % (int(time.time() * 1000) % 1000)
    _out_q.put("%s  %s" % (t, text))


def out_flush(timeout=1.0):
    end = time.monotonic() + timeout
    while not _out_q.empty() and time.monotonic() < end:
        time.sleep(0.01)


# ---------------------------------------------------------------- клиент

class Client:
    def __init__(self, port, key, fmt, monitor=False, verbose=False, data_max=0):
        self.port = port
        self.key = key
        self.fmt = fmt
        self.data_max = data_max
        self.monitor = monitor
        self.verbose = verbose

        self.parser = FrameParser()
        self.tx_lock = threading.Lock()
        self.stop = threading.Event()
        self.junk_line = bytearray()

        self.quiet = False          # не отвечать на пинги
        self.tamper_next = False

        self.reset_session()

    # --- состояние сеанса

    def reset_session(self):
        self.session = False
        self.accepted = False       # ESP ответил FID_ACCEPT: сеанс открыт, можно слать данные
        self.esp_instance = None
        self.my_instance = None
        self.n_tx = 0
        self.esp_rx = 0
        self.rnd = b"\x00" * 4
        self.esp_session = b"\x00" * 4
        self.beacon_interval = 5000
        self.last_esp = 0.0
        self.last_frame = None
        self.last_n = None

    def connect(self, iv, beacon):
        epoch, inst, counter, n, _ = iv_parts(iv)
        self.session = True
        self.accepted = False
        self.esp_instance = inst
        self.my_instance = inst ^ 0xFF
        self.n_tx = n
        self.esp_rx = n
        self.esp_session = bytes(iv[8:12])  # идентификатор сеанса ESP: в AAD наших кадров и сверка его кадров
        self.rnd = os.urandom(4)
        self.beacon_interval = beacon["interval"] or 5000
        self.last_esp = time.monotonic()
        self.log("ПОДКЛЮЧЕНО: SN %s «%s», epoch %d, counter %d, instance ESP 0x%02X, наш 0x%02X, сеанс ESP %s, наш random %s"
                 % (beacon["sn"], beacon["name"], epoch, counter, inst, self.my_instance, self.esp_session.hex(), self.rnd.hex()))
        if not self.monitor:
            # Подключение = beacon + FID_ACCEPT. ESP открывает сеанс и отвечает своим FID_ACCEPT
            self.send(pack(bytes([FID_ACCEPT])), note="accept: открыть сеанс")

    # --- вывод

    def log(self, text):
        out(text)

    def on_junk(self, data):
        self.junk_line += data
        while b"\n" in self.junk_line:
            line, _, rest = bytes(self.junk_line).partition(b"\n")
            self.junk_line = bytearray(rest)
            text = line.decode("utf-8", "replace").rstrip("\r")
            if text.strip():
                self.log("[esp] " + text)
        if len(self.junk_line) > 256:
            self.log("[мусор] " + hx(bytes(self.junk_line)))
            self.junk_line.clear()

    # --- приём

    def on_frame(self, f: Frame):
        tag = "%s %s" % ("←" if f.direction == DIR_FROM else "→?", FMT_NAMES.get(f.fmt, "fmt%d" % f.fmt))
        if self.verbose:
            self.log("кадр %s: %s" % (tag, hx(f.raw)))

        if f.version != VERSION:
            self.log("%s: версия %d, ожидалась %d — пропуск" % (tag, f.version, VERSION))
            return
        if f.direction != DIR_FROM:
            self.log("%s: направление «к ESP» — это наш кадр (эхо линии?), пропуск" % tag)
            return

        if not f.secure:
            if f.fmt != FMT_RAW:
                self.log("%s: неизвестный формат, пропуск" % tag)
                return
            # На RAW-канале (формат отправки RAW) RAW-кадры ESP — обычные кадры сеанса; на прочих — недоверенные
            raw_channel = self.fmt == FMT_RAW and self.session
            self.show_messages(tag + ("" if raw_channel else " (без подписи)"), f.payload, None, trusted=raw_channel)
            return

        if self.key is None:
            epoch, inst, counter, n, _ = iv_parts(f.iv)
            self.log("%s: без ключа, epoch %d counter %d instance 0x%02X" % (tag, epoch, counter, inst))
            self.show_messages(tag + " (не проверено)", f.payload[IV_LEN:-TAG_LEN], None, trusted=False)
            return

        try:
            data = f.open(self.key)
        except Exception:
            self.log("%s: ПОДПИСЬ НЕ СОШЛАСЬ (чужой ключ или подделка)" % tag)
            return

        epoch, inst, counter, n, rnd = iv_parts(f.iv)
        msgs = split_messages(data)
        is_beacon = bool(msgs) and msgs[0][0] == FID_BEACON

        if not self.session:
            if self.monitor:
                self.show_messages("%s N=%d:%d" % (tag, epoch, counter), data, f.iv, trusted=True)
                return
            if is_beacon and parse_beacon(msgs[0][1]) is not None:
                self.show_messages("%s N=%d:%d" % (tag, epoch, counter), data, f.iv, trusted=True)
                self.connect(f.iv, parse_beacon(msgs[0][1]))
            else:
                self.log("%s: ждём beacon, пакет N=%d:%d пропущен" % (tag, epoch, counter))
            return

        if inst != self.esp_instance:
            self.log("%s: instance 0x%02X вместо 0x%02X — отклонено" % (tag, inst, self.esp_instance))
            return
        if rnd != self.esp_session:
            # Другой идентификатор сеанса: ESP закрыл наш сеанс или перезапустился. Beacon с верной подписью
            # и новым идентификатором — подключаемся заново по нему. Проверяется ДО N: если эпоха ESP не успела
            # сохраниться, N нового запуска может быть меньше esp_rx. Остальные кадры с чужим идентификатором
            # отбрасываем. Подсунутый старый beacon переподключит к старому идентификатору; ESP не примет наши
            # пакеты, а следующий настоящий beacon (снова другой идентификатор) вернёт всё на место
            if is_beacon and parse_beacon(msgs[0][1]) is not None:
                self.log("%s: сеанс ESP сменился (%s -> %s) — переподключение"
                         % (tag, self.esp_session.hex(), rnd.hex()))
                self.show_messages("%s N=%d:%d" % (tag, epoch, counter), data, f.iv, trusted=True)
                self.connect(f.iv, parse_beacon(msgs[0][1]))
            else:
                self.log("%s: сеанс %s не текущий (%s) — отклонено" % (tag, rnd.hex(), self.esp_session.hex()))
            return

        if n <= self.esp_rx:
            self.log("%s: ПОВТОР/СТАРЫЙ N=%d:%d (последний принятый %d:%d) — отклонено"
                     % (tag, epoch, counter, self.esp_rx >> 32, self.esp_rx & 0xFFFFFFFF))
            return

        self.esp_rx = n
        self.last_esp = time.monotonic()
        self.show_messages("%s N=%d:%d" % (tag, epoch, counter), data, f.iv, trusted=True)

    def show_messages(self, prefix, data, iv, trusted):
        for fid, msg in split_messages(data):
            if fid == FID_BEACON and parse_beacon(msg) is None:
                self.log("%s BEACON битый (длина %d): %s" % (prefix, len(msg), hx(msg)))
            elif fid == FID_BEACON:
                b = parse_beacon(msg)
                sid = iv[8:12].hex() if iv is not None else "?"
                self.log("%s BEACON SN %s «%s» interval %d мс, сеанс %s"
                         % (prefix, b["sn"], b["name"], b["interval"], sid))
                if trusted and self.session and iv is not None:
                    self.beacon_interval = b["interval"] or self.beacon_interval
            elif fid == FID_ACCEPT:
                self.log("%s ACCEPT" % prefix)
                if trusted and self.session and not self.accepted:
                    self.accepted = True
                    self.log("СЕАНС ОТКРЫТ: ESP подтвердил подключение")
            elif fid == FID_PING:
                self.log("%s PING" % prefix)
                if trusted and self.session and not self.monitor:
                    if self.quiet:
                        self.log("  (не отвечаем: /quiet)")
                    else:
                        self.send(pack(bytes([FID_PING])), note="ответ на пинг")
            else:
                self.log("%s DATA fId 0x%02X: %s" % (prefix, fid, hx(msg)))

    # --- отправка

    def next_iv(self, n=None, instance=None):
        if n is None:
            if self.n_tx >= N_MAX:
                raise RuntimeError("N исчерпан")
            self.n_tx += 1
            n = self.n_tx
        return make_iv(n, self.my_instance if instance is None else instance, self.rnd), n

    def send(self, data, fmt=None, note="", n=None, key=None, instance=None, direction=DIR_TO,
             tamper=False, bad_crc=False, bind=None):
        fmt = self.fmt if fmt is None else fmt
        if fmt != FMT_RAW:
            if not self.session:
                self.log("нет сеанса — ждём beacon (для RAW сеанс не нужен)")
                return
            if self.key is None:
                self.log("нет ключа — доступен только RAW")
                return
        iv = None
        nn = None
        if fmt != FMT_RAW:
            iv, nn = self.next_iv(n, instance)
        if self.data_max and len(data) > self.data_max:
            self.log("DATA %d байт больше лимита канала %d — не отправлено" % (len(data), self.data_max))
            return
        frame = bytearray(build_frame(direction, fmt, data, key or self.key, iv,
                                      self.esp_session if bind is None else bind))
        if tamper or self.tamper_next:
            # портим байт DATA (или последний байт тега, если DATA пустая), CRC пересчитываем,
            # чтобы кадр дошёл до проверки подписи
            pos = HEADER_LEN + (IV_LEN if fmt != FMT_RAW else 0)
            if pos >= len(frame) - CRC_LEN:
                pos = len(frame) - CRC_LEN - 1
            frame[pos] ^= 0x01
            frame[-2:] = struct.pack("<H", crc16_xmodem(bytes(frame[:-2])))
            self.tamper_next = False
        if bad_crc:
            frame[-1] ^= 0xFF
        frame = bytes(frame)
        with self.tx_lock:
            self.port.write(frame)
        if not tamper and not bad_crc and key is None and instance is None and direction == DIR_TO and n is None and bind is None:
            self.last_frame = frame
            self.last_n = nn
        where = "" if nn is None else " N=%d:%d" % (nn >> 32, nn & 0xFFFFFFFF)
        self.log("→ %s%s %s%s" % (FMT_NAMES[fmt], where, hx(data), ("  (" + note + ")") if note else ""))
        if self.verbose:
            self.log("  кадр: " + hx(frame))

    # --- потоки

    def reader(self):
        while not self.stop.is_set():
            try:
                chunk = self.port.read(self.port.in_waiting or 1)
            except Exception as e:
                self.log("ошибка порта: %s" % e)
                self.stop.set()
                return
            if getattr(self.port, "take_disconnect", None) and self.port.take_disconnect():
                # BLE: соединение порвалось. Сеанс и недособранный кадр — в прошлом, ждём beacon нового подключения
                self.reset_session()
                self.parser = FrameParser()
                continue
            for kind, item in self.parser.feed(chunk) if chunk else self.parser.feed(b""):
                if kind == "frame":
                    try:
                        self.on_frame(item)
                    except Exception as e:
                        self.log("ошибка обработки кадра: %r" % e)
                elif kind == "crc":
                    self.log("кадр с битым CRC: %s" % hx(item))
                else:
                    self.on_junk(item)
            if self.session and not self.monitor:
                limit = 3 * self.beacon_interval / 1000.0
                if time.monotonic() - self.last_esp > limit:
                    self.log("СВЯЗЬ ПОТЕРЯНА: от ESP ничего %.1f с — ждём beacon" % limit)
                    self.reset_session()

    def status(self):
        if not self.session:
            self.log("сеанса нет, ждём beacon; формат %s" % FMT_NAMES[self.fmt])
            return
        self.log(("сеанс подтверждён, " if self.accepted else "ждём accept от ESP, ") + "сеанс ESP %s: наш N %d:%d, instance 0x%02X, random %s; последний N ESP %d:%d; формат %s; quiet=%s"
                 % (self.esp_session.hex(), self.n_tx >> 32, self.n_tx & 0xFFFFFFFF, self.my_instance, self.rnd.hex(),
                    self.esp_rx >> 32, self.esp_rx & 0xFFFFFFFF, FMT_NAMES[self.fmt], self.quiet))

    def command(self, line):
        line = line.strip()
        if not line:
            return
        if not line.startswith("/") and line not in ("help", "?"):
            try:
                data = pack(*[bytes.fromhex(part.replace(",", " ")) for part in line.split("|")])
            except ValueError:
                self.log("не hex или пустое сообщение. help — список команд")
                return
            if self.session and not self.accepted and not self.monitor:
                self.log("ESP ещё не подтвердил подключение (accept) — данные не отправлены")
                return
            self.send(data)
            return

        parts = line.split()
        cmd = parts[0].lstrip("/").lower()
        arg = parts[1:]
        payload = pack(bytes.fromhex("".join(arg[1:])) if len(arg) > 1 else b"\x10\xAA")

        if cmd in ("help", "?"):
            print(__doc__)
        elif cmd == "quit":
            self.stop.set()
        elif cmd == "status":
            self.status()
        elif cmd == "fmt":
            if not arg or arg[0].lower() not in FMT_BY_NAME:
                self.log("/fmt raw|sign|encrypt")
                return
            self.fmt = FMT_BY_NAME[arg[0].lower()]
            self.log("формат отправки: %s" % FMT_NAMES[self.fmt])
        elif cmd == "resync":
            self.reset_session()
            self.log("сеанс сброшен, ждём beacon")
        elif cmd == "quiet":
            self.quiet = not self.quiet
            self.log("ответы на пинги %s" % ("ВЫКЛЮЧЕНЫ" if self.quiet else "включены"))
        elif cmd == "replay":
            if self.last_frame is None:
                self.log("ещё ничего не отправлено")
                return
            with self.tx_lock:
                self.port.write(self.last_frame)
            self.log("→ ПОВТОР последнего кадра байт в байт (ESP должен отклонить: N не новее)")
        elif cmd == "tamper":
            self.send(payload, tamper=True, note="испорчен байт данных, CRC верный — ESP должен отклонить по тегу")
        elif cmd == "stale":
            if self.last_n is None:
                self.log("ещё ничего не отправлено подписанным")
                return
            self.send(payload, n=self.last_n, note="новые данные со старым N — ESP должен отклонить как повтор")
        elif cmd == "badkey":
            self.send(payload, key=os.urandom(32), note="чужой ключ — ESP должен отклонить по тегу")
        elif cmd == "badinst":
            self.send(payload, instance=self.esp_instance, note="instance не инвертирован — ESP должен отклонить")
        elif cmd == "dir":
            self.send(payload, direction=DIR_FROM, note="направление «от ESP» — ESP должен отклонить")
        elif cmd == "crc":
            self.send(payload, bad_crc=True, note="битый CRC — ESP должен отклонить")
        elif cmd == "oldboot":
            self.send(payload, bind=os.urandom(4),
                      note="подписан под чужой идентификатор сеанса (как пакет закрытого сеанса или прошлого запуска) — ESP должен отклонить по тегу")
        elif cmd == "flood":
            count = int(arg[0]) if arg else 100
            t0 = time.monotonic()
            for i in range(count):
                self.send(pack(bytes([0x10, i & 0xFF])), note="flood %d/%d" % (i + 1, count))
            self.log("отправлено %d пакетов за %.3f с" % (count, time.monotonic() - t0))
        else:
            self.log("неизвестная команда. help — список")


def load_key(args):
    text = None
    if args.key:
        text = args.key
    elif args.key_file:
        with open(args.key_file, "r", encoding="ascii") as fh:
            text = fh.read()
    if text is None:
        return None
    text = "".join(text.split()).replace(":", "")
    key = bytes.fromhex(text)
    if len(key) != 32:
        sys.exit("ключ должен быть 32 байта (64 hex-символа), получено %d" % len(key))
    return key


def main():
    ap = argparse.ArgumentParser(description="L3-клиент ESP32 по UART или BLE")
    ap.add_argument("--port", help="UART: COM5, /dev/ttyUSB0 ...")
    ap.add_argument("--baud", type=int, default=921600, help="скорость (по умолчанию как в L3DriverDashboard: 921600)")
    ap.add_argument("--ble", action="store_true", help="BLE вместо UART: устройство ищется по рекламе")
    ap.add_argument("--address", help="BLE: адрес устройства (иначе — по рекламе, при нескольких спросит)")
    ap.add_argument("--sn", help="BLE: SN устройства, 16 hex-символов")
    ap.add_argument("--key", help="ключ, 64 hex-символа")
    ap.add_argument("--key-file", help="файл с ключом в hex")
    ap.add_argument("--format", choices=sorted(FMT_BY_NAME), help="формат отправки (по умолчанию UART — sign, BLE — encrypt)")
    ap.add_argument("--data-max", type=int, default=0, help="лимит DATA канала, байт (0 — без лимита, как UART)")
    ap.add_argument("--monitor", action="store_true", help="только слушать: без сеанса и ответов на пинги")
    ap.add_argument("--verbose", "-v", action="store_true", help="печатать кадры целиком в hex")
    args = ap.parse_args()
    if not args.ble and not args.port:
        ap.error("нужен --port (UART) или --ble")
    fmt_name = args.format or ("encrypt" if args.ble else "sign")

    key = load_key(args)
    if args.ble:
        port = BlePort(args.address, args.sn, log=out)
        dev = port.find()
        where = "BLE %s" % dev.address
    else:
        if serial is None:
            sys.exit("Нужен pyserial:  pip install pyserial cryptography")
        port = serial.Serial(args.port, args.baud, timeout=0.02)
        where = "порт %s @ %d" % (args.port, args.baud)
    client = Client(port, key, FMT_BY_NAME[fmt_name], monitor=args.monitor, verbose=args.verbose, data_max=args.data_max)
    client.log("%s, формат %s, ключ %s%s" % (where, FMT_NAMES[client.fmt],
                                           "задан" if key else "НЕ задан (только RAW и просмотр)",
                                           ", режим монитора" if args.monitor else ""))
    if args.ble:
        port.connect(dev)
    client.log("ждём beacon... (help — команды)")

    th = threading.Thread(target=client.reader, daemon=True)
    th.start()
    try:
        while not client.stop.is_set():
            try:
                line = input()
            except EOFError:
                # stdin закрыт (например, запуск из скрипта): просто слушаем
                while not client.stop.is_set():
                    time.sleep(0.2)
                break
            client.command(line)
    except KeyboardInterrupt:
        pass
    client.stop.set()
    th.join(timeout=1)
    port.close()
    out_flush()


if __name__ == "__main__":
    main()
