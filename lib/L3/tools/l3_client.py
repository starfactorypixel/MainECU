#!/usr/bin/env python3
"""
L3-клиент для ESP32 по UART: подключение по beacon, ответы на пинги, отправка данных
в форматах RAW / SIGN / ENCRYPT и проверочные «атаки» (повтор, подделка, чужой ключ...).

Зависимости:  pip install pyserial cryptography

Примеры:
    python l3_client.py --port COM5 --key 00112233...eeff
    python l3_client.py --port /dev/ttyUSB0 --key-file key.hex --format encrypt
    python l3_client.py --port COM5 --monitor            # только слушать, без ключа тоже можно

Команды в консоли (help — список):
    10 01 02 ff            отправить DATA (hex) текущим форматом; первый байт — fId сообщения
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
import struct
import sys
import threading
import time

try:
    import serial
except ImportError:
    sys.exit("Нужен pyserial:  pip install pyserial cryptography")
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
DATA_MAX = 64
PAYLOAD_MAX = IV_LEN + DATA_MAX + TAG_LEN      # 92
CRC_LEN = 2

DIR_FROM = 0    # от ESP
DIR_TO = 1      # к ESP

FMT_RAW, FMT_SIGN, FMT_ENCRYPT = 0, 1, 3
FMT_NAMES = {FMT_RAW: "RAW", FMT_SIGN: "SIGN", FMT_ENCRYPT: "ENCRYPT"}
FMT_BY_NAME = {"raw": FMT_RAW, "sign": FMT_SIGN, "encrypt": FMT_ENCRYPT}

N_MAX = (1 << 56) - 1

FID_BEACON = 0x00
FID_PING = 0x01
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


def split_messages(data: bytes):
    """DATA -> [(fId, bytes)]. Транспортные fId режутся по длине, прочее — одним куском до конца DATA."""
    out = []
    i = 0
    while i < len(data):
        fid = data[i]
        if fid == FID_BEACON:
            if len(data) - i < BEACON_HEAD.size:
                out.append((fid, data[i:]))     # обрезанный beacon: покажется как ошибка разбора
                break
            size = BEACON_HEAD.size + data[i + BEACON_HEAD.size - 1]
            out.append((fid, data[i:i + size]))
            i += size
        elif fid == FID_PING:
            out.append((fid, data[i:i + 1]))
            i += 1
        else:
            out.append((fid, data[i:]))
            break
    return out


def parse_beacon(msg: bytes):
    """Beacon переменной длины: 13 байт заголовка + length байт имени. None — сообщение битое."""
    if len(msg) < BEACON_HEAD.size:
        return None
    fid, interval, sn, length = BEACON_HEAD.unpack_from(msg)
    if length > BEACON_NAME_MAX or len(msg) != BEACON_HEAD.size + length:
        return None
    name = msg[BEACON_HEAD.size:].decode("utf-8", "replace")
    return {"interval": interval, "sn": sn.hex(), "name": name}


def hx(b: bytes) -> str:
    return b.hex(" ")


# ---------------------------------------------------------------- клиент

class Client:
    def __init__(self, port, key, fmt, monitor=False, verbose=False):
        self.port = port
        self.key = key
        self.fmt = fmt
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
            # Подключение = beacon + первый пакет: ESP открывает сеанс и перестаёт менять его идентификатор
            self.send(b"", note="первый пакет: открыть сеанс")

    # --- вывод

    def log(self, text):
        t = time.strftime("%H:%M:%S") + ".%03d" % (int(time.time() * 1000) % 1000)
        print("%s  %s" % (t, text), flush=True)

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
            self.show_messages(tag + " (без подписи)", f.payload, None, trusted=False)
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
            elif fid == FID_PING:
                self.log("%s PING" % prefix)
                if trusted and self.session and not self.monitor:
                    if self.quiet:
                        self.log("  (не отвечаем: /quiet)")
                    else:
                        self.send(bytes([FID_PING]), note="ответ на пинг")
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
        self.log("сеанс ESP %s: наш N %d:%d, instance 0x%02X, random %s; последний N ESP %d:%d; формат %s; quiet=%s"
                 % (self.esp_session.hex(), self.n_tx >> 32, self.n_tx & 0xFFFFFFFF, self.my_instance, self.rnd.hex(),
                    self.esp_rx >> 32, self.esp_rx & 0xFFFFFFFF, FMT_NAMES[self.fmt], self.quiet))

    def command(self, line):
        line = line.strip()
        if not line:
            return
        if not line.startswith("/") and line not in ("help", "?"):
            try:
                data = bytes.fromhex(line.replace(",", " "))
            except ValueError:
                self.log("не hex. help — список команд")
                return
            self.send(data)
            return

        parts = line.split()
        cmd = parts[0].lstrip("/").lower()
        arg = parts[1:]
        payload = bytes.fromhex("".join(arg[1:])) if len(arg) > 1 else b"\x10\xAA"

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
                self.send(bytes([0x10, i & 0xFF]), note="flood %d/%d" % (i + 1, count))
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
    ap = argparse.ArgumentParser(description="L3-клиент ESP32 по UART")
    ap.add_argument("--port", required=True, help="COM5, /dev/ttyUSB0 ...")
    ap.add_argument("--baud", type=int, default=921000, help="скорость (по умолчанию как в L3DriverDashboard: 921000)")
    ap.add_argument("--key", help="ключ, 64 hex-символа")
    ap.add_argument("--key-file", help="файл с ключом в hex")
    ap.add_argument("--format", default="sign", choices=sorted(FMT_BY_NAME), help="формат отправки")
    ap.add_argument("--monitor", action="store_true", help="только слушать: без сеанса и ответов на пинги")
    ap.add_argument("--verbose", "-v", action="store_true", help="печатать кадры целиком в hex")
    args = ap.parse_args()

    key = load_key(args)
    port = serial.Serial(args.port, args.baud, timeout=0.02)
    client = Client(port, key, FMT_BY_NAME[args.format], monitor=args.monitor, verbose=args.verbose)
    client.log("порт %s @ %d, формат %s, ключ %s%s" % (args.port, args.baud, FMT_NAMES[client.fmt],
                                                         "задан" if key else "НЕ задан (только RAW и просмотр)",
                                                         ", режим монитора" if args.monitor else ""))
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


if __name__ == "__main__":
    main()
