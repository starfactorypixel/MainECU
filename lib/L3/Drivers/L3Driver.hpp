#pragma once
#include <inttypes.h>
#include <string.h>
#include "Drivers/L3DriverInterface.hpp"
#include "L3ManagerInterface.hpp"
#include "Packets/L3PacketTransport.hpp"
#include "Packets/L3PacketSecurityRaw.hpp"
#include "Packets/L3PacketPayload.hpp"

class L3Driver : public L3DriverInterface
{
	
	struct state_t
	{
		dev_state_t state;			// Состояние устройства
		struct
		{
			uint32_t time;			// Время последнего полученного пинга
			uint8_t retries;		// Остаток попыток получить ответ на пинг
		} ping;
	};

	

	public:

		L3Driver() = default;
		L3Driver(const L3Driver &) = delete;
		L3Driver &operator=(const L3Driver &) = delete;

		void Init(L3ManagerInterface *manager)
		{
			_manager = manager;
			Reset();

			return;
		}

		dev_type_t GetType()
		{
			return _dev_type;
		}

		// Добавляем программный пакет на отправку
		void AddPacket(const uint8_t *data, uint16_t length)
		{
			// Тут копируем данные в payload L3PacketSecurity до тех пор, пока они влезают.
			// Если больше не влезают, то вызываем Commit() чтобы зафиксировать набранный пакет и отправить его

			// Внутрь метода добавить проверку, что даже в пустой пакет у нас не влезают данные и возвращать отдельный код ошибки.
			// чтобы не пробовать отправлять данные больше payload.
			bool result = _security_tx.AddPayload(data, length);
			if(result == false)
			{
				// Не вставили потому что нету места, значит фиксируем пакет и отправляем его

				Commit();

				// Вставляем данные в уже новый подготовленный пакет
				_security_tx.AddPayload(data, length);
			}

		}

		void Commit()
		{
			// Говорим пакету что нужно выполнить все операции и подготовить пакет к отправке
			uint8_t *data;
			uint16_t length = _security_tx.GetPacketPtr(data);
			// Проблема. Даже если payload пустой GetPacketPtr вернёт длину с пустым payload. Это не ошибка но зачем слать пустые пакеты?
			if(length > 0)
			{
				bool result = _transport_tx.PutPayload(data, length);
				if(result == true)
				{
					uint8_t *data2;
					uint16_t length2 = _transport_tx.GetPacketPtr(data2);
					if(length2 > 0)
					{
						SendData(data2, length2);
						// Тут у нас массив байт для отправки в нужный порт
					}
				}

				_security_tx.Init();
			}
		}

		void Reset()
		{
			memset(&_state, 0x00, sizeof(_state));
			_state.state = DEVSTATE_IDLE;
			_transport_rx.Init();
			_transport_tx.Init();
			_security_rx.Init();
			_security_tx.Init();
			_payload_rx.Init();

			return;
		}

		// Пробуем копировать Transport в Security пакет
		void AttemptCopyTransportToSecurityPacket()
		{
			// Метод GetPayloadPtr() устроен так, что он вернёт длину 0, если пакет не готов
			// Поэтому безопастно дёргать этот метод всегда чтобы убедиться что пакет собран

			uint8_t *data;
			uint16_t length = _transport_rx.GetPayloadPtr(data);
			if(length > 0)
			{
				// Тут нужно копировать в _security пакет полученный Payload и вставлять в буфер RX пакетов
				_security_rx.PutPacketPtr(data, length);
				
				uint8_t *data2;
				uint16_t length2 = _security_rx.GetPayloadPtr(data2);
				if(length2 > 0)
				{
					_payload_rx.Add(data2, length2);
				}

				_transport_rx.Init();
			}

			return;
		}


		// Каким-то образом пришёл 1 байт. Прерывание или рунтайм
		void RxByte(uint8_t rx, uint32_t rx_time)
		{
			_transport_rx.AddPacketByte(rx, rx_time);
			AttemptCopyTransportToSecurityPacket();

			return;
		}

		// Каким-то образом пришли несколько байт. Прерывание или рунтайм
		void RxBytes(const uint8_t *rx, uint16_t rx_len/*, uint32_t rx_time*/)
		{
			_transport_rx.PutPacketBytes(rx, rx_len, 0/*(esp_timer_get_time() / 1000ULL)*/);							// Провайдер времени.
			AttemptCopyTransportToSecurityPacket();
		}

		void Processing(uint32_t time)
		{
			// Постоянно проверяем наличие принятых пакетов из буфера. Вытащить из буфера и выполнить.
			// Пока без буфера а 1 объект

			while(true)
			{
				uint8_t *data;
				uint16_t length = _payload_rx.Get(data);

				if(length == 0) break;
				
				uint16_t offset = 0;
				while(offset < length)
				{
					uint16_t processed = _manager->RxPacket(_dev_type, data + offset, length - offset);
					if(processed == 0)
						break;
						
					offset += processed;
				}

				_payload_rx.Delete();
			}
			
			return;
		}


	protected:
		
		L3PacketSecurityRaw _security_tx;
		L3PacketTransport _transport_tx;

		
		L3PacketTransport _transport_rx;
		L3PacketSecurityRaw _security_rx;
		L3PacketPayload _payload_rx;

		L3ManagerInterface *_manager;

		dev_type_t _dev_type;
		state_t _state;

	private:
		
		//virtual void SendData(const uint8_t *data, uint16_t length) = 0;
};
