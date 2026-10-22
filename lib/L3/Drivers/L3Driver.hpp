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
	- сеанс открывается первым принятым аутентифицированным пакетом клиента (клиент шлёт его сразу после
	  подключения), с этого момента идентификатор не меняется;
	- пока пакеты клиента идут, пинги не отправляются;
	- пауза _cfg.idle мс -> пинг, далее каждые _cfg.ping_interval мс, всего _cfg.ping_retries штук;
	- любой валидный пакет клиента (в том числе ответ на пинг) сбрасывает отсчёт;
	- нет ответа на последний пинг -> сеанс закрыт: новый идентификатор и OnSessionClose().
	  Все пакеты закрытого сеанса (в том числе недоставленные) перестают проходить проверку подписи,
	  клиент видит новый идентификатор в следующем beacon и подключается заново.
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

		void AddPacket(const uint8_t *data, uint16_t length)
		{
			if(_packet_tx.AddPayload(data, length)) return;

			// Нет места в текущем пакете: отправляем накопленное и пробуем в пустой
			if(_packet_tx.GetError() == L3C::ERROR_TX_NO_SPACE)
			{
				Commit();
				if(_packet_tx.AddPayload(data, length)) return;
			}

			// ERROR_TX_DATA_TOO_BIG и прочее: данные отброшены
			DEBUG_LOG_TOPIC("L3New", "AddPacket error: %s\n", L3C::ErrorName(_packet_tx.GetError()));
		}

		void Commit()
		{
			if(_packet_tx.PayloadIsSet() == false) return;

			_Send();
		}

		// Сброс канала: настройки (_cfg) сохраняются, состояние и разбор приёма — нет.
		// Открытый сеанс закрывается без OnSessionClose(): Reset вызывается и до Open()
		void Reset()
		{
			_seq.NewSession();

			memset(&_state, 0x00, sizeof(_state));
			_state.state = DEVSTATE_IDLE;

			_packet_tx.Clear();		// недособранный исходящий пакет выбрасывается, формат канала сохраняется
			_packet_rx.Reset();
			_state.rx_count = _packet_rx.GetValidCount();

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
			// Активность клиента: счётчик валидных пакетов изменился
			uint32_t count = _packet_rx.GetValidCount();
			if(count != _state.rx_count)
			{
				_state.rx_count = count;
				_state.rx_time = time;
				_state.pings = 0;

				if(_state.state != DEVSTATE_ACTIVE)
				{
					_state.state = DEVSTATE_ACTIVE;
					OnSessionOpen();
				}
			}

			// Принятые данные
			while(_packet_rx.IsReady())
			{
				uint8_t *data;
				uint16_t length = _packet_rx.GetPayloadPtr(data);
				// Здесь только формат канала (_format): RAW возможен лишь на канале, где его разрешил драйвер

				_Dispatch(data, length);
				_packet_rx.PopPayload();
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
			uint16_t ping_interval = 1000;		// Интервал между пингами, мс
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

		// Сеанс открыт: первый валидный пакет клиента после IDLE/TIMEOUT
		virtual void OnSessionOpen() {}

		// Сеанс закрыт по пингам. BT: разорвать соединение. UART/USB: ничего, достаточно состояния
		virtual void OnSessionClose() {}

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

		struct state_t
		{
			dev_state_t state;			// IDLE — сеанса не было, ACTIVE — открыт, TIMEOUT — закрыт по пингам
			uint32_t rx_count;			// Последнее значение _packet_rx.GetValidCount()
			uint32_t rx_time;			// Время последнего валидного пакета клиента, мс
			uint32_t ping_time;			// Время последнего пинга, мс
			uint8_t pings;				// Отправлено пингов без ответа
			uint32_t beacon_time;		// Время последнего beacon, мс
		};

		void _Send()
		{
			uint8_t *data;
			uint16_t length = _packet_tx.GetPacketPtr(data);
			if(length > 0)
				SendData(data, length);
			else
				DEBUG_LOG_TOPIC("L3New", "Send error: %s\n", L3C::ErrorName(_packet_tx.GetError()));

			_packet_tx.Clear();		// всегда: при ошибке пакет теряется, но очередь не блокируется, формат сохраняется

			return;
		}

		// Разбор DATA на сообщения. Транспортные fId обрабатываются здесь, остальные — в L3Manager
		void _Dispatch(const uint8_t *data, uint16_t length)
		{
			uint16_t offset = 0;
			while(offset < length)
			{
				const uint8_t *msg = data + offset;
				uint16_t left = length - offset;
				uint16_t processed;

				switch(msg[0])
				{
					// Ответ клиента на пинг: активность уже учтена по счётчику валидных пакетов
					case L3Fid::FID_PING:	processed = sizeof(L3Fid::ping_t); break;

					// Beacon от клиента не ожидается: остаток DATA отбрасываем
					case L3Fid::FID_BEACON:	processed = 0; break;

					default:				processed = _manager->RxPacket(this, msg, left); break;
				}

				if(processed == 0 || processed > left) break;

				offset += processed;
			}

			return;
		}

		void _CheckLink(uint32_t time)
		{
			if(_state.pings == 0)
			{
				if(time - _state.rx_time < _cfg.idle) return;
			}
			else
			{
				if(time - _state.ping_time < _cfg.ping_interval) return;

				if(_state.pings >= _cfg.ping_retries)
				{
					_CloseSession();
					return;
				}
			}

			_state.pings++;
			_state.ping_time = time;
			_SendPing();

			return;
		}

		void _CloseSession()
		{
			_seq.NewSession();
			_state.state = DEVSTATE_TIMEOUT;
			_state.pings = 0;
			OnSessionClose();

			return;
		}

		void _SendPing()
		{
			const L3Fid::ping_t ping = {L3Fid::FID_PING};

			Commit();	// накопленное уходит отдельно, пинг — своим пакетом
			AddPacket((const uint8_t *)&ping, sizeof(ping));
			Commit();

			return;
		}

		void _SendBeacon()
		{
			Commit();	// сначала накопленное: beacon идёт через тот же объект и затёр бы его

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
			_packet_tx.PutPayload((const uint8_t *)&beacon, L3Fid::BEACON_HEAD_LEN + beacon.length);	// без хвоста имени
			_Send();

			_packet_tx.CfgFormat(_format);

			return;
		}

		state_t _state = {};
};
