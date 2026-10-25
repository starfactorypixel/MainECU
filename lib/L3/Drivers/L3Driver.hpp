#pragma once
#include <inttypes.h>
#include <string.h>
#include "Drivers/L3DriverInterface.hpp"
#include "L3ManagerInterface.hpp"
#include "Packets/L3PacketRx.hpp"
#include "Packets/L3PacketTx.hpp"
#include "Packets/L3Fid.hpp"

/*
Канал связи с одним клиентом.

Beacon: каждые _cfg.beacon_interval мс, всегда SIGN, независимо от сеанса, без хвоста имени.

Сеанс. Его идентификатор (L3PacketSeq::Session) клиент видит в IV beacon и подписывает им свои пакеты:
	- пока сеанса нет, каждый beacon несёт новый идентификатор;
	- сеанс открывается только сообщением FID_ACCEPT от клиента (он шлёт его сразу после beacon), ESP отвечает
	  своим FID_ACCEPT; с этого момента идентификатор не меняется. Без сеанса остальные кадры отбрасываются;
	- пока пакеты клиента идут, пинги не отправляются;
	- пауза _cfg.idle мс -> пинг, далее каждые _cfg.ping_interval мс, всего _cfg.ping_retries штук;
	- любой валидный пакет клиента (в том числе ответ на пинг) сбрасывает отсчёт;
	- нет ответа на последний пинг -> сеанс закрыт (CLOSE_TIMEOUT): новый идентификатор, события.
	  Все пакеты закрытого сеанса (в том числе недоставленные) перестают проходить проверку подписи,
	  клиент видит новый идентификатор в следующем beacon и подключается заново;
	- ping_interval == 0 -> пингов нет, сеанс закрывает только транспорт (_TransportClosed, CLOSE_TRANSPORT).
*/
class L3Driver : public L3DriverInterface
{
	public:

		L3Driver() = default;
		L3Driver(const L3Driver &) = delete;
		L3Driver &operator=(const L3Driver &) = delete;

		void Attach(L3ManagerInterface *manager)
		{
			_manager = manager;
			Reset();

			return;
		}

		dev_type_t GetType()
		{
			return _dev_type;
		}

		dev_state_t GetState()
		{
			return _state.state;
		}

		bool IsSessionActive()
		{
			return _state.state == DEVSTATE_ACTIVE;
		}

		// Данные приложения. Без открытого сеанса отбрасываются: false
		bool AddPacket(const uint8_t *data, uint16_t length)
		{
			if(IsSessionActive() == false) return false;

			return _AddPacket(data, length);
		}

		// Отправить накопленное. Без открытого сеанса ничего не делает
		void Commit()
		{
			if(IsSessionActive() == false) return;

			_Commit();
		}

		// AddPacket + Commit
		bool AddPacketCommit(const uint8_t *data, uint16_t length)
		{
			if(AddPacket(data, length) == false) return false;

			_Commit();

			return true;
		}

		// Сброс канала: настройки (_cfg) сохраняются, состояние и разбор приёма — нет.
		// Открытый сеанс закрывается без событий: Reset вызывается и до Open(), и самим приложением
		void Reset()
		{
			_seq.NewSession();

			memset(&_state, 0x00, sizeof(_state));
			_state.state = DEVSTATE_IDLE;
			_transport_closed = false;

			_packet_tx.Clear();		// недособранный исходящий пакет выбрасывается, формат канала сохраняется
			_packet_rx.Reset();
			_state.rx_count = _packet_rx.GetValidCount();
			_state.reject_count = _packet_rx.GetRejectedCount();

			return;
		}

		// Каким-то образом пришёл 1 байт. Прерывание или рунтайм
		void RxByte(uint8_t rx, uint32_t rx_time)
		{
			_packet_rx.AddPacketByte(rx, rx_time);

			return;
		}

		// Каким-то образом пришли несколько байт (целый кадр). Прерывание или рунтайм
		void RxBytes(const uint8_t *rx, uint16_t rx_len, uint32_t rx_time)
		{
			_packet_rx.PutPacketBytes(rx, rx_len, rx_time);

			return;
		}

		void Processing(uint32_t time)
		{
			// Ошибки приёма: задача приёма только считает, сообщаем здесь. Между двумя вызовами
			// могло быть несколько — сообщается последняя
			uint32_t rejects = _packet_rx.GetRejectedCount();
			if(rejects != _state.reject_count)
			{
				_state.reject_count = rejects;
				_Error(_packet_rx.GetLastReject());
			}

			// Активность клиента (только в сеансе): счётчик валидных пакетов изменился
			uint32_t count = _packet_rx.GetValidCount();
			if(count != _state.rx_count)
			{
				_state.rx_count = count;
				if(_state.state == DEVSTATE_ACTIVE)
				{
					_state.rx_time = time;
					_state.pings = 0;
				}
			}

			// Принятые данные. Без сеанса разбирается только FID_ACCEPT
			while(_packet_rx.IsReady())
			{
				uint8_t *data;
				uint16_t length = _packet_rx.GetPayloadPtr(data);
				// Здесь только формат канала (_format): RAW возможен лишь на канале, где его разрешил драйвер

				_Dispatch(data, length, time);
				_packet_rx.PopPayload();
			}

			// Обрыв от транспорта — после приёма: данные, пришедшие до обрыва, уже отданы
			if(_transport_closed)
			{
				_transport_closed = false;
				if(_state.state == DEVSTATE_ACTIVE)
					_CloseSession(CLOSE_TRANSPORT);
			}

			if(_state.state == DEVSTATE_ACTIVE)
				_CheckLink(time);

			if(_cfg.beacon_interval > 0 && time - _state.beacon_time >= _cfg.beacon_interval)
			{
				_state.beacon_time = time;
				_SendBeacon();
			}

			return;
		}


	protected:

		// Настройки канала, задаются в конструкторе драйвера. Reset() их не трогает
		struct cfg_t
		{
			uint16_t beacon_interval = 0;		// Интервал beacon, мс. 0 — beacon выключен
			uint16_t idle = 1000;				// Пауза в пакетах клиента до первого пинга, мс
			uint16_t ping_interval = 1000;		// Интервал между пингами, мс. 0 — пинги выключены
			uint8_t ping_retries = 3;			// Пингов без ответа до закрытия сеанса
		};

		// Формат канала. Вызывается один раз, в конструкторе драйвера
		void _CfgFormat(L3C::format_t format)
		{
			_format = format;
			_packet_tx.CfgFormat(format);
			_packet_rx.CfgFormat(format);

			return;
		}

		// Лимит DATA канала, байт (BLE, LoRa — по размеру кадра транспорта). Вызывается один раз, в конструкторе
		// драйвера. 0 или не вызван — без лимита (UART): сколько вмещает формат, до PACKET_DATA_LEN_MAX.
		// Меньше beacon с полным именем нельзя: поднимается до DATA_MIN_BEACON
		void _CfgDataMax(uint16_t max)
		{
			if(max != 0 && max < DATA_MIN_BEACON) max = DATA_MIN_BEACON;

			_packet_tx.CfgDataMax(max);
			_packet_rx.CfgDataMax(max);

			return;
		}

		// Протокольная часть Open(): ключ, счётчики, запуск. Транспортную часть драйвер делает сам.
		// key и epoch — указатели в Config (эпоху Config сохраняет сам, асинхронно).
		// key == nullptr — канал без защиты (RAW, beacon выключен): счётчики не запускаются.
		// Ошибка уходит и в менеджер (Error), и в результат
		L3C::error_t _OpenChannel(const uint8_t *key, uint32_t *epoch)
		{
			_packet_tx.CfgKey(key, _dev_type, &_seq);
			_packet_rx.CfgKey(key, _dev_type, &_seq);

			if(key == nullptr) return L3C::ERROR_NONE;

			_seq.Cfg(epoch);
			L3C::error_t err = _seq.Start();
			if(err != L3C::ERROR_NONE) _Error(err);	// подписанные пакеты отправляться и приниматься не будут

			return err;
		}

		// Транспорт сообщил об обрыве (BT disconnect, TCP закрыт). Можно вызывать из любого контекста:
		// здесь только флаг, сеанс закроется в ближайшем Processing() (CLOSE_TRANSPORT)
		void _TransportClosed()
		{
			_transport_closed = true;

			return;
		}

		// Сеанс открыт: принят FID_ACCEPT клиента
		virtual void OnSessionOpen() {}

		// Сеанс закрыт. BT по CLOSE_TIMEOUT: разорвать соединение. UART/USB: ничего, достаточно состояния
		virtual void OnSessionClose(close_reason_t reason) { (void)reason; }

		cfg_t _cfg;
		L3PacketSeq _seq;								// Общее для Rx и Tx состояние счётчиков канала
		// Формат канала (задаётся _CfgFormat): в нём идут исходящие и только он принимается (beacon — всегда SIGN).
		// UART — SIGN, LoRa — ENCRYPT, сервер через HTTPS — RAW (целостность даёт TLS)
		L3C::format_t _format = L3C::FORMAT_SIGN;

		L3PacketRx _packet_rx;
		L3PacketTx _packet_tx;

		L3ManagerInterface *_manager = nullptr;

		dev_type_t _dev_type = DEVTYPE_NONE;

	private:

		static constexpr uint16_t DATA_MIN_BEACON = 1 + sizeof(L3Fid::beacon_t);	// [len] + beacon с именем 32 байта = 45

		struct state_t
		{
			dev_state_t state;			// IDLE — сеанса нет, ACTIVE — открыт, TIMEOUT — закрыт по пингам
			uint32_t rx_count;			// Последнее значение _packet_rx.GetValidCount()
			uint32_t reject_count;		// Последнее значение _packet_rx.GetRejectedCount()
			uint32_t rx_time;			// Время последнего валидного пакета клиента, мс
			uint32_t ping_time;			// Время последнего пинга, мс
			uint8_t pings;				// Отправлено пингов без ответа
			uint32_t beacon_time;		// Время последнего beacon, мс
		};

		void _Error(L3C::error_t error)
		{
			if(_manager != nullptr) _manager->Error(this, error);

			return;
		}

		// Без проверки сеанса: для своих сообщений драйвера (пинг) и приложения через AddPacket()
		bool _AddPacket(const uint8_t *data, uint16_t length)
		{
			if(_packet_tx.AddPayload(data, length)) return true;

			// Нет места в текущем пакете: отправляем накопленное и пробуем в пустой
			if(_packet_tx.GetError() == L3C::ERROR_TX_NO_SPACE)
			{
				_Commit();
				if(_packet_tx.AddPayload(data, length)) return true;
			}

			// ERROR_TX_DATA_TOO_BIG и прочее: данные отброшены
			_Error(_packet_tx.GetError());

			return false;
		}

		void _Commit()
		{
			if(_packet_tx.PayloadIsSet() == false) return;

			_Send();
		}

		void _Send()
		{
			uint8_t *data;
			uint16_t length = _packet_tx.GetPacketPtr(data);
			if(length > 0)
				SendData(data, length);
			else
				_Error(_packet_tx.GetError());

			_packet_tx.Clear();		// всегда: при ошибке пакет теряется, но очередь не блокируется, формат сохраняется

			return;
		}

		// Разбор DATA на сообщения. Транспортные fId обрабатываются здесь, остальные — в L3Manager
		// DATA: [len][fId][данные…] … Сообщение — len байт после байта длины. Неверный len — остаток пропустить
		void _Dispatch(const uint8_t *data, uint16_t length, uint32_t time)
		{
			const uint8_t *p = data;
			uint16_t left = length;

			while(left > 0)
			{
				uint8_t n = p[0];
				if(n == 0 || 1 + n > left) break;

				const uint8_t *msg = p + 1;		// msg[0] — fId

				// Без сеанса принимается только FID_ACCEPT: остальное отбрасывается
				if(_state.state != DEVSTATE_ACTIVE && msg[0] != L3Fid::FID_ACCEPT) break;

				switch(msg[0])
				{
					case L3Fid::FID_ACCEPT:	_Accept(time); break;
					case L3Fid::FID_PING:	break;			// ответ на пинг: активность учтена по счётчику валидных
					case L3Fid::FID_BEACON:	break;			// beacon от клиента не ожидается: пропустить
					default:				_manager->RxPacket(this, msg, n); break;
				}

				p += 1 + n;
				left -= 1 + n;
			}

			return;
		}

		// Клиент подключается: открыть сеанс (если ещё не открыт) и ответить FID_ACCEPT.
		// Ответ уходит с ближайшим Commit (менеджер делает его после Processing), вместе с другими ответами
		void _Accept(uint32_t time)
		{
			if(_state.state != DEVSTATE_ACTIVE)
			{
				_state.state = DEVSTATE_ACTIVE;
				_state.rx_time = time;
				_state.pings = 0;

				OnSessionOpen();
				if(_manager != nullptr) _manager->SessionOpen(this);
			}

			const L3Fid::accept_t accept = {L3Fid::FID_ACCEPT};
			_AddPacket((const uint8_t *)&accept, sizeof(accept));

			return;
		}

		void _CheckLink(uint32_t time)
		{
			if(_cfg.ping_interval == 0) return;		// пинги выключены: сеанс закрывает только транспорт

			if(_state.pings == 0)
			{
				if(time - _state.rx_time < _cfg.idle) return;
			}
			else
			{
				if(time - _state.ping_time < _cfg.ping_interval) return;

				if(_state.pings >= _cfg.ping_retries)
				{
					_CloseSession(CLOSE_TIMEOUT);
					return;
				}
			}

			_state.pings++;
			_state.ping_time = time;
			_SendPing();

			return;
		}

		void _CloseSession(close_reason_t reason)
		{
			_seq.NewSession();
			_state.state = (reason == CLOSE_TIMEOUT) ? DEVSTATE_TIMEOUT : DEVSTATE_IDLE;
			_state.pings = 0;
			_packet_tx.Clear();		// недоотправленное в закрытом сеансе не должно уйти в следующем

			OnSessionClose(reason);
			if(_manager != nullptr) _manager->SessionClose(this, reason);

			return;
		}

		void _SendPing()
		{
			const L3Fid::ping_t ping = {L3Fid::FID_PING};

			_AddPacket((const uint8_t *)&ping, sizeof(ping));	// вместе с накопленными данными, если влезает
			_Commit();

			return;
		}

		void _SendBeacon()
		{
			_Commit();	// сначала накопленное: beacon идёт через тот же объект и затёр бы его

			const auto &cfg = Config::body.security;	// ссылка, не копия (в копию попадал бы ключ)
			L3Fid::beacon_t beacon = {};
			beacon.fId = L3Fid::FID_BEACON;
			beacon.interval = _cfg.beacon_interval;
			memcpy(beacon.sn, cfg.sn, sizeof(beacon.sn));
			memcpy(beacon.name, cfg.name, sizeof(beacon.name));
			beacon.length = (uint8_t)strnlen((const char *)beacon.name, sizeof(beacon.name));

			// Beacon всегда SIGN: из его IV клиент берёт стартовый N и идентификатор сеанса.
			// Пока сеанса нет — новый идентификатор: подсунутый старый beacon заведомо с чужим.
			// SyncTx: N beacon выше всего принятого, новый клиент не попадёт на отбрасывание как повтор
			if(_state.state != DEVSTATE_ACTIVE) _seq.NewSession();
			_seq.SyncTx();
			_packet_tx.CfgFormat(L3C::FORMAT_SIGN);
			_packet_tx.AddPayload((const uint8_t *)&beacon, L3Fid::BEACON_HEAD_LEN + beacon.length);	// [len][beacon], без хвоста имени
			_Send();

			_packet_tx.CfgFormat(_format);

			return;
		}

		state_t _state = {};
		volatile bool _transport_closed = false;	// пишет транспорт (любой контекст), читает Processing()
};
