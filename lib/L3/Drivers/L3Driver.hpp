#pragma once
#include <inttypes.h>
#include <string.h>
#include "Drivers/L3DriverInterface.hpp"
#include "L3ManagerInterface.hpp"
#include "Packets/L3PacketRx.hpp"
#include "Packets/L3PacketTx.hpp"

class L3Driver : public L3DriverInterface
{
	
	struct state_t
	{
		dev_state_t state;			// Состояние устройства
		
		struct
		{
			uint8_t retries;		// Остаток попыток получить ответ на пинг
			uint32_t prevtime;		// Время последнего полученного пинга
		} ping;

		struct
		{
			uint16_t interval;		// Интервал генерация beacon пакета, мс
			uint32_t prevtime;		// Время последней оправки beacon пакета, мс
		} beacon;
	};

	

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

			uint8_t *data;
			uint16_t length = _packet_tx.GetPacketPtr(data);
			if(length > 0)
				SendData(data, length);
			else
				DEBUG_LOG_TOPIC("L3New", "Commit error: %s\n", L3C::ErrorName(_packet_tx.GetError()));

			_packet_tx.Clear();		// всегда: при ошибке пакет теряется, но очередь не блокируется, формат сохраняется
		}

		void Reset()
		{
			uint16_t interval = _state.beacon.interval;		// memset не должен обнулять настройку
			memset(&_state, 0x00, sizeof(_state));
			_state.state = DEVSTATE_IDLE;
			_state.beacon.interval = interval;

			_packet_tx.CfgFormat(_tx_format);
			_packet_rx.Reset();

			return;
		}

		// Каким-то образом пришёл 1 байт. Прерывание или рунтайм
		void RxByte(uint8_t rx, uint32_t rx_time)
		{
			_packet_rx.AddPacketByte(rx, rx_time);

			return;
		}

		// Каким-то образом пришли несколько байт. Прерывание или рунтайм
		void RxBytes(const uint8_t *rx, uint16_t rx_len/*, uint32_t rx_time*/)
		{
			_packet_rx.PutPacketBytes(rx, rx_len, 0/*(esp_timer_get_time() / 1000ULL)*/);							// Провайдер времени.
		}

		void Processing(uint32_t time)
		{
			// Постоянно проверяем наличие принятых пакетов из буфера. Вытащить из буфера и выполнить.
			// Пока без буфера а 1 объект

			while(_packet_rx.IsReady())
			{
				uint8_t *data;
				uint16_t length = _packet_rx.GetPayloadPtr(data);
				// _packet_rx.GetPayloadFormat() == FORMAT_RAW — данные не аутентифицированы
				// (при политике по умолчанию RAW сюда не попадает)

				uint16_t offset = 0;
				while(offset < length)
				{
					uint16_t processed = _manager->RxPacket(this, data + offset, length - offset);
					if(processed == 0) break;

					offset += processed;
				}

				_packet_rx.PopPayload();
			}

			if(_state.beacon.interval > 0 && time - _state.beacon.prevtime > _state.beacon.interval)
			{
				_state.beacon.prevtime = time;
				_SendBeacon();
			}
			
			return;
		}


	protected:
		
		L3PacketSeq _seq;								// Общее для Rx и Tx состояние счётчиков линка
		L3C::format_t _tx_format = L3C::FORMAT_SIGN;	// Формат исходящих по умолчанию (по умолчанию подписанный, не RAW)

		L3PacketRx _packet_rx;
		L3PacketTx _packet_tx;
	

		L3ManagerInterface *_manager;

		dev_type_t _dev_type;
		state_t _state;

	private:

		void _SendBeacon()
		{
			Commit();	// сначала накопленное: beacon идёт через тот же объект и затёр бы его

			const auto &cfg = Config::body.security;	// ссылка, не копия (в копию попадал бы ключ)
			packet_beacon_t beacon = {};
			memcpy(beacon.sn, cfg.sn, sizeof(beacon.sn));
			memcpy(beacon.name, cfg.name, sizeof(beacon.name));

			// Beacon всегда SIGN: из его IV телефон берёт стартовые epoch/counter
			_packet_tx.CfgFormat(L3C::FORMAT_SIGN);
			_packet_tx.PutPayload((uint8_t *)&beacon, sizeof(beacon));
			Commit();

			_packet_tx.CfgFormat(_tx_format);

			return;
		}
		
		structpack packet_beacon_t
		{
			uint8_t fId = 0x00;
			uint8_t length;
			uint8_t sn[8];
			uint8_t name[32];
		};
};
