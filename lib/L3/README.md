# L3 — защищённый канал между ESP32 и клиентами

Библиотека прикладного протокола L3 на ESP32: приборная панель по UART, телефон по BLE, в будущем LoRa и сервер. Один менеджер, по драйверу на канал. Кадры подписываются (AES-256-GCM) или шифруются, повторы и подделка отсекаются, сеанс с клиентом открывается и закрывается сам.

Этот файл — про устройство библиотеки на стороне ESP: менеджер, драйверы и как с ними работать. Сам протокол (кадр, подпись, сеанс, сообщения) и руководство для разработчика клиента — в [`docs/L3_PROTOCOL.md`](docs/L3_PROTOCOL.md).

## Платформа

- ESP32-S3, Arduino-ESP32 **2.0.17** (ESP-IDF 4.4.7). Сборка Arduino идёт в **C++11** (`-std=gnu++11`): код библиотеки под это рассчитан.
- Крипто: `mbedtls` из IDF (GCM), случайные числа: `esp_random()`.
- FreeRTOS: у драйверов свои задачи приёма (UART) или задача стека Bluetooth (BLE).
- BLE: Bluedroid, встроенный в ядро Arduino, через функции IDF (`esp_ble_gap_*`, `esp_ble_gatts_*`). Обёртки Arduino `BLEDevice` / `BLEServer` не используются.
- [CUtils](https://github.com/starfactorypixel/Library_CUtils): `CUtils_CRC16.h` и `CUtils_RingBuffer.h`. Нужна версия кольцевого буфера **с барьерами памяти и методами `WriteSlot` / `CommitWrite` / `ReadSlot`** — на ней держится очередь приёма.
- От проекта нужны:
  - `Config::Obj().security`: `sn[8]`, `name[32]` (UTF-8, без завершающего нуля, если имя ровно 32 байта), `devs[8]` с полями `key[32]` и `epoch` (`uint32_t`). Индекс в `devs` — номер бита типа драйвера: UART — `devs[0]`, BLE — `devs[1]`. Эпоху `Config` сохраняет сам.
  - Макрос `DEBUG_LOG_TOPIC(topic, fmt, ...)`.

## Структура

| Файл | Назначение |
|---|---|
| `L3Manager.hpp` | **Менеджер — то, с чем работает приложение.** Держит драйверы, раздаёт события, рассылает пакеты по маске каналов |
| `L3ManagerInterface.hpp` | Интерфейс менеджера для драйвера: сообщения, открытие и закрытие сеанса, ошибки |
| `L3Fid.hpp` | Идентификаторы и структуры сообщений: beacon, accept, ping, подписка, CanMsg, BlockInfo, BlockErr |
| `Drivers/L3DriverInterface.hpp` | Типы драйверов (маска), состояния, причины закрытия; `Open()` и `SendData()` |
| `Drivers/L3Driver.hpp` | **Общая логика канала:** сеанс, beacon, пинги, разбор DATA, отправка. Основа любого драйвера |
| `Drivers/L3DriverDashboard.hpp` | Драйвер UART: приборная панель |
| `Drivers/L3DriverBLE.hpp` | Драйвер BLE: телефон |
| `Packets/L3PacketConsts.hpp` | Константы кадра, форматы, коды ошибок (`L3C::`) |
| `Packets/L3Packet.hpp` | Кадр: сборка, подпись и шифрование GCM, проверка |
| `Packets/L3PacketRx.hpp` | Приём: разбор потока или целых кадров, все проверки, очередь принятого |
| `Packets/L3PacketTx.hpp` | Передача: накопление сообщений в кадр, подпись и выдача готового кадра |
| `Packets/L3PacketSeq.hpp` | Счётчики N, эпоха и идентификатор сеанса канала (общие для приёма и передачи) |
| `Packets/L3PacketPayload.hpp` | Очередь принятых DATA: 64 кадра на драйвер |
| `docs/L3_PROTOCOL.md` | Описание протокола и руководство для клиента |
| `tools/l3_client.py` | Клиент для ПК (UART и BLE) для проверки устройства |
| `tools/l3_test_vectors.json` | Тестовые векторы протокола |

## Как это работает

```
Приём (каждый драйвер отдельно):

  транспорт ──► задача приёма ──► L3PacketRx ──────────────────► очередь DATA (64) ──► Processing() в loop
  (UART, BLE)   (RxTask / BTC)    CRC → формат → подпись/шифр          │                   │
                                  → instance → сеанс → N               │                   ├─ ACCEPT, PING, BEACON — сам драйвер
                                  отклонено → счётчик и код ошибки ─────┘                   └─ остальное ─► callbacks.rx

Передача:

  AddPacket(msg) ──► L3PacketTx: [len][msg][len][msg]… ──► Commit ──► IV, N, GCM, CRC ──► SendData() драйвера
  (не влезло — накопленное уходит отдельным кадром само)         (в конце каждого Processing менеджер делает Commit сам)
```

| Что | Где выполняется |
|---|---|
| Приём байт, проверка кадра (CRC, подпись или расшифровка, N, сеанс), запись в очередь | задача приёма драйвера (UART — `RxTask`, BLE — задача Bluetooth) |
| Разбор DATA, сеанс, пинги, beacon, все колбэки приложения | `Processing()` в loop |
| Сборка кадра, подпись, `SendData()` | там, где вызван `Commit` / `AddPacketCommit`, обычно loop |
| Флаги транспорта (`_TransportOpened` / `_TransportClosed`) | любой контекст: только флаг, обработка в `Processing()` |

**Сеанс** (подробно — в описании протокола, разделы 6–7):
- ESP раз в `beacon_interval` шлёт подписанный beacon: SN, имя, интервал; в IV — идентификатор сеанса.
- Клиент отвечает accept → сеанс открыт (`callbacks.open`), ESP отвечает своим accept.
- Без сеанса драйвер отбрасывает всё, кроме accept; `AddPacket` возвращает `false`.
- Пока клиент молчит дольше `idle`, ESP шлёт пинги; нет ответа на `ping_retries` штук — сеанс закрыт (`CLOSE_TIMEOUT`).
- Транспорт сообщил об обрыве — сеанс закрыт (`CLOSE_TRANSPORT`).
- Закрытие сеанса = новый идентификатор: кадры закрытого сеанса больше не проходят проверку подписи.

**Транспорт с подключением** (`_cfg.link`, BLE): клиента нет, пока транспорт не открыт. Beacon идут только между `_TransportOpened()` и `_TransportClosed()`; первый — сразу при открытии, интервал отсчитывается от него.

## Использование

```cpp
#include "L3Manager.hpp"
#include "Drivers/L3DriverDashboard.hpp"
#include "Drivers/L3DriverBLE.hpp"

uint32_t MyMillis() { return esp_timer_get_time() / 1000ULL; }

// Сообщение приложения: msg[0] — fId, length — fId + данные. Неизвестный fId — просто пропустить
void L3Rx(L3Driver *driver, const uint8_t *msg, uint16_t length)
{
	switch(msg[0])
	{
		case L3Fid::FID_SUBSCRIBE: /* ... */ break;
		case L3Fid::FID_CAN_MSG:   /* ... */ break;
		default: break;
	}
}

void L3Open(L3Driver *driver) { DEBUG_LOG_TOPIC("L3", "Session open, dev 0x%02X\n", driver->GetType()); }
void L3Close(L3Driver *driver, L3DriverInterface::close_reason_t reason) { DEBUG_LOG_TOPIC("L3", "Session close, dev 0x%02X, reason %u\n", driver->GetType(), reason); }
void L3Error(L3Driver *driver, L3C::error_t error) { DEBUG_LOG_TOPIC("L3", "Error, dev 0x%02X: %s\n", driver->GetType(), L3C::ErrorName(error)); }

const L3Manager::callbacks_t l3_callbacks = {&L3Open, &L3Close, &L3Rx, &L3Error};	// любой может быть nullptr
L3Manager manager(l3_callbacks, &MyMillis);
L3DriverDashboard dash;
L3DriverBLE ble;

void setup()
{
	// Config уже загружен: SN, имя, ключи и эпохи каналов
	manager.AddDriver(dash);	// Attach + Open: ключ, счётчики, запуск транспорта
	manager.AddDriver(ble);
}

void loop()
{
	manager.Processing();		// приём, сеанс, пинги, beacon; в конце — Commit каждого канала
}

// Отправка: в один или несколько каналов (маска), только туда, где открыт сеанс
void SendState(uint16_t can_id, const uint8_t *data, uint8_t len)
{
	L3Fid::can_msg_t msg;
	msg.can_id = can_id;
	msg.SetData(data, len);
	manager.AddPacket(L3DriverInterface::DEVTYPE_ALL, (const uint8_t *)&msg, msg.GetLen());
	// уйдёт в ближайшем Processing(); сразу — manager.Commit(...) или AddPacketCommit(...)
}
```

Сообщения одного действия складывайте подряд через `AddPacket`: они уйдут одним кадром (кадр добавляет 36 байт к данным). Длину перед каждым сообщением драйвер добавляет сам.

## Сообщения (`L3Fid.hpp`)

| fId | Структура | Направление |
|---|---|---|
| `FID_BEACON` 0x00 | `beacon_t` | ESP → клиент. Шлёт драйвер, приложение не видит |
| `FID_ACCEPT` 0x01 | `accept_t` | обе стороны. Обрабатывает драйвер |
| `FID_PING` 0x05 | `ping_t` | обе стороны. Обрабатывает драйвер |
| `FID_SUBSCRIBE` 0x12 | `subscribe_t` | клиент → ESP. В эфире `2 + 2 × count` байт, не вся структура |
| `FID_SCRIPT_CTRL` 0x1B | — | обе стороны, см. протокол, раздел 10 |
| `FID_BLOCK_INFO` 0x1D | `block_info_t` | ESP → клиент |
| `FID_BLOCK_ERR` 0x1E | `block_error_t` | ESP → клиент |
| `FID_CAN_MSG` 0x35 | `can_msg_t` | обе стороны. Длина — `GetLen()` |

`MSG_MAX` — наибольшее сообщение (fId + данные): `min(PACKET_DATA_LEN_MAX − 1, 255)`. Сообщение длиннее отклоняется (`ERROR_TX_DATA_TOO_BIG`).

## API

### `L3Manager`

| Метод | Описание |
|---|---|
| `L3Manager(callbacks, millis)` | Колбэки приложения и функция времени в мс |
| `AddDriver(driver)` | Подключить драйвер и запустить его (`Open()`). Один драйвер на тип |
| `GetDriver(dev)` | Драйвер по типу (один бит маски) или `nullptr` |
| `Processing()` | Вызывать в loop как можно чаще. По всем драйверам: `Processing(time)`, затем `Commit()` |
| `AddPacket(dev, data, len)` | Добавить сообщение в каналы маски `dev` (например `DEVTYPE_ALL`). Канал без сеанса его пропускает |
| `Commit(dev)` | Отправить накопленное в каналах маски сейчас |
| `AddPacketCommit(dev, data, len)` | `AddPacket` + `Commit` |
| `Reset(dev)` | Сброс каналов маски: сеанс закрывается без событий, очереди очищаются (см. «Важно») |

Колбэки (`callbacks_t`) вызываются только из runtime: из `Processing()`, а `error` ещё из `AddPacket` / `Commit` (ошибка отправки) и `AddDriver` (ошибка запуска). Из задачи приёма — никогда.

| Колбэк | Когда |
|---|---|
| `open(driver)` | Сеанс открыт: клиент прислал accept |
| `close(driver, reason)` | Сеанс закрыт: `CLOSE_TIMEOUT` — пинги без ответа, `CLOSE_TRANSPORT` — обрыв транспорта |
| `rx(driver, msg, length)` | Сообщение приложения |
| `error(driver, error)` | Отклонённый кадр (`ERROR_RX_*`), ошибка отправки (`ERROR_TX_*`) или запуска (`ERROR_SEQ_*`, `ERROR_CFG_*`) |

### `L3Driver` (общее для всех драйверов)

| Метод | Описание |
|---|---|
| `GetType()` | Тип драйвера (бит маски) |
| `GetState()` | `DEVSTATE_IDLE` — сеанса нет, `DEVSTATE_ACTIVE` — открыт, `DEVSTATE_TIMEOUT` — закрыт по пингам |
| `IsSessionActive()` | Сеанс открыт |
| `AddPacket(data, len)` | Добавить сообщение. Без сеанса — `false` |
| `Commit()` | Отправить накопленное. Без сеанса ничего не делает |
| `AddPacketCommit(data, len)` | `AddPacket` + `Commit` |
| `Reset()` | Сброс канала: сеанс закрывается без событий, настройки (`_cfg`) сохраняются |
| `Processing(time)` | Вызывает менеджер |

## Ошибки (`L3C::error_t`)

Имя ошибки в текст: `L3C::ErrorName(e)`. Об отклонённых кадрах задача приёма только считает, а сообщает `Processing()`: если между двумя вызовами их было несколько, придёт последняя.

| Группа | Коды | Смысл |
|---|---|---|
| Настройка и счётчики | `CFG_NO_KEY`, `CFG_NO_SEQ`, `CFG_FORMAT`, `SEQ_NOT_STARTED`, `SEQ_EXHAUSTED` | Канал не запущен. `SEQ_EXHAUSTED` при старте — эпоха 0xFFFFFF и больше: нужен новый ключ (или не инициализирован `Config`: на стёртой памяти там `FF`) |
| Передача | `TX_DATA_TOO_BIG`, `TX_NO_SPACE`, `TX_CRYPTO`, `TX_MSG_EMPTY`, `TX_SEALED` | Сообщение не отправлено |
| Приём: кадр | `RX_FRAME_TOO_SHORT/LONG`, `RX_HEAD`, `RX_VERSION`, `RX_DIRECTION`, `RX_PAYLOAD_LEN_MAX`, `RX_LEN_MISMATCH`, `RX_CRC` | Мусор, помеха, чужой кадр |
| Приём: формат | `RX_FORMAT_RESERVED`, `RX_FORMAT_DENIED`, `RX_PAYLOAD_LEN_MIN`, `RX_DATA_LEN` | Не тот формат канала или DATA больше лимита канала |
| Приём: защита | `RX_IV_INSTANCE`, `RX_IV_REPLAY`, `RX_AUTH`, `RX_CRYPTO` | Чужой канал, повтор, неверный ключ или подделка, кадр закрытого сеанса |
| Приём: очередь | `RX_QUEUE_FULL` | Очередь DATA переполнена: `Processing()` вызывается слишком редко |

## Свой драйвер

Наследник `L3Driver`. Пример — `L3DriverDashboard` (поток байт) и `L3DriverBLE` (кадр целиком, транспорт с подключением).

**Конструктор:**

| Что | Описание |
|---|---|
| `_dev_type` | Тип из `dev_type_t` — один бит. По нему менеджер хранит драйвер, по нему же берётся `Config::...devs[__builtin_ctz(type)]` |
| `_cfg.beacon_interval` | Интервал beacon, мс. 0 — без beacon |
| `_cfg.idle`, `_cfg.ping_interval`, `_cfg.ping_retries` | Пауза до первого пинга, интервал пингов, сколько без ответа до закрытия. `ping_interval = 0` — без пингов: сеанс закрывает только транспорт |
| `_cfg.link` | Транспорт с подключением: beacon только при открытом транспорте |
| `_CfgFormat(fmt)` | Формат канала: `FORMAT_SIGN`, `FORMAT_ENCRYPT` или `FORMAT_RAW` (только если целостность даёт транспорт, например TLS). Один раз |
| `_CfgDataMax(n)` | Лимит DATA канала, если транспорт не пропускает кадр полной длины. Один раз |
| `_packet_rx.CfgTimeout(ms)` | Для потока байт: недособранный кадр старше этого считается оборванным |

**Методы:**

| Метод | Что делает драйвер |
|---|---|
| `Open()` | Взять `key` и `epoch` **ссылкой** из `Config` и вызвать `_OpenChannel(key, &epoch)`; затем запустить транспорт |
| `SendData(data, len)` | Отправить готовый кадр **целиком**, одной записью. Вызывается из runtime: не должен надолго блокировать |
| Приём: поток байт | `_packet_rx.PutStream(buf, len, time_ms)` из задачи приёма. Сам находит кадры, мусор и обрывы. После потери данных (переполнение FIFO) — `_packet_rx.DropFrame()` |
| Приём: кадр целиком | `RxBytes(frame, len, time_ms)` |
| `_TransportOpened()` / `_TransportClosed()` | Транспорт готов или оборвался (BT, TCP). Из любого контекста |
| `OnSessionOpen()` / `OnSessionClose(reason)` | Необязательные хуки. Например, BLE по `CLOSE_TIMEOUT` разрывает соединение |

Правило для приёма: **один писатель — одна задача приёма** на драйвер. Очередь DATA рассчитана ровно на одного писателя и одного читателя (`Processing()`).

## Драйверы в комплекте

**`L3DriverDashboard` — UART:**
- UART0, 921600 8N1, TX — GPIO44, RX — GPIO43, формат SIGN, beacon раз в 5 с;
- задача `L3UART_RX` забирает всё из буфера драйвера UART и отдаёт в `PutStream`, тайм-аут сборки кадра — 5 мс;
- отправка: `uart_wait_tx_done` (ждёт, пока уйдёт предыдущий кадр, чтобы между кадрами была пауза), затем `uart_write_bytes`.

**`L3DriverBLE` — BLE:**
- Bluedroid, формат ENCRYPT, одно соединение;
- реклама legacy: manufacturer data `FF FF` + `PX` + SN, имя в ответе на сканирование; во время соединения к рекламе подключиться нельзя (устройство видно как занятое);
- сервис Nordic UART: RX (…0002) — запись, TX (…0003) — уведомления; одна запись или одно уведомление — один кадр;
- транспорт открыт, когда телефон подключён, включил уведомления и MTU не меньше `PACKET_LEN_MAX + 3`; ESP сразу шлёт beacon;
- сеанс не открылся за 10 с после подключения или закрыт по пингам — ESP разрывает соединение;
- после обрыва снова рекламируется для подключения.

## Важно

- **`Reset()`** очищает очередь приёма: вызывать только когда задача приёма этого канала не пишет в неё. У BLE после `Reset()` при живом соединении beacon не пойдут до переподключения; соединение без сеанса само разорвётся через 10 с.
- **BLE-драйвер может быть только один:** колбэки Bluedroid — функции C без контекста, драйвер хранит указатель на себя.
- **Время отправки.** UART: при кадрах подряд `SendData` ждёт, пока уйдёт предыдущий кадр (кадр 164 байта — около 1,8 мс на 921600, предел ожидания — 5 мс). BLE: обычно сразу; если буферы стека Bluetooth заняты, до 10 повторов по 1 мс, затем кадр теряется.
- **Очередь приёма — 64 кадра на драйвер.** На UART в предельном режиме это больше 100 мс потока; если `Processing()` вызывается реже, кадры теряются (`RX_QUEUE_FULL`).
- **Эпоха** растёт на 1 при каждом запуске канала и сохраняется `Config` асинхронно. Ключ и эпоха берутся из `Config` по ссылке: копировать `Config` нельзя (ключ остался бы в копии, а эпоха не сохранялась бы).
- **Новый канал в `Config`** нужно инициализировать при первом запуске: случайный `key`, `epoch = 0`. На стёртой памяти `epoch = 0xFFFFFFFF`, и канал не запустится (`SEQ_EXHAUSTED`).
- **C++11.** Сборка Arduino 2.0.17 — `gnu++11`: в заголовках нельзя брать адрес у `static constexpr` массивов (нет определения без строки в .cpp) и нельзя `inline`-переменные.
- **`PACKET_DATA_LEN_MAX`** (128) задаёт наибольший кадр. Изменить можно, но тогда у клиентов должен быть тот же DATA_MAX, а для BLE — достаточный MTU.
- **Проверка без телефона:** `tools/l3_client.py --port COMx` (UART) или `--ble` (нужен адаптер Bluetooth 4.0+ с поддержкой BLE), см. протокол, раздел 12.
