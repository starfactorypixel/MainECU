#pragma once
#include <inttypes.h>
#include "L3PacketConsts.hpp"
#include "L3PacketTransport.hpp"
#include "L3PacketSecurityRaw.hpp"

#define structpack		struct __attribute__((packed))



class ManagerInterface
{
	public:
		virtual ~ManagerInterface() = default;
		
		virtual uint16_t RxPacket(uint8_t dev, const uint8_t *rx, uint16_t rx_len) = 0;
};


// Драйвер устройства. Каждый драйвер наследуется от этого класса, но пока будет один
class Driver
{
	public:

		void Init(ManagerInterface *manager)
		{
			_manager = manager;

			return;
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
			if(length > 0)
			{
				bool result = _transport_tx.PutPayload(data, length);
				if(result == true)
				{
					uint8_t *data2;
					uint16_t length2 = _transport_tx.GetPacketPtr(data2);
					if(length2 > 0)
					{
						// Тут у нас массив байт для отправки в нужный порт
					}
				}

				_security_tx.Init();
			}
		}


		// Каким-то образом пришёл 1 байт. Прерывание или рунтайм
		void RxByte(uint8_t rx, uint32_t rx_time)
		{
			_transport_rx.AddPacketByte(rx, rx_time);
			
			uint8_t *data;
			uint16_t length = _transport_rx.GetPayloadPtr(data);
			if(length > 0)
			{
				// Тут нужно копировать в _security пакет полученный Payload и вставлять в буфер RX пакетов
				_security_rx.PutPacketPtr(data, length);
			}

			return;
		}

		// Каким-то образом пришли несколько байт. Прерывание или рунтайм
		void RxBytes(const uint8_t *rx, uint16_t rx_len, uint32_t rx_time)
		{
			_transport_rx.PutPacketBytes(rx, rx_len, rx_time);
		}

		void Processing(uint32_t time)
		{
			// Постоянно проверяем наличие принятых пакетов из буфера. Вытащить из буфера и выполнить.
			// Пока без буфера а 1 объект

			uint8_t *data;
			uint16_t length = _security_rx.GetPayloadPtr(data);
			
			uint16_t offset = 0;
			while(offset < length)
			{
				uint16_t processed = _manager->RxPacket(_dev, data + offset, length - offset);
				if(processed == 0)
					break;
					
				offset += processed;
			}
			
			return;
		}


	private:
		
		L3PacketSecurityRaw _security_tx;
		L3PacketTransport _transport_tx;

		L3PacketSecurityRaw _security_rx;
		L3PacketTransport _transport_rx;

		ManagerInterface *_manager;

		uint8_t _dev;
};





// Общий менеджер для отправки и приёма данных из L3 устройств
// Для каждого устроиства создаётся отдельный объект-драйвер
class Manager : public ManagerInterface
{
	public:

		Manager()
		{

		}

		void AddDriver(Driver &driver)
		{
			if(_driver_idx >= 8) return;
			
			_driver[_driver_idx++] = &driver;
			driver.Init(this);
			
			return;
		}

		// Метод вызывается когда пришёл готовый пакет.
		virtual uint16_t RxPacket(uint8_t dev, const uint8_t *rx, uint16_t rx_len) override
		{
			// Пришёл пакет
			// В rx_len указана максимальная длина которую можно обработать. это НЕ длина пакета, это >= длина пакета. Но хорошо бы убедиться что это ещё и не < пакета.
			
			uint8_t fId = rx[0];
			// Работа с rx

			// В конце необходимо вернуть фактическую длину пакета, которую вычислили базе fId.
			return 6;
		}

		// Добавляем программный пакет на отправку
		void AddPacket(uint8_t dev, const uint8_t *data, uint16_t length)
		{
			// Ищем по маске dev нужный драйвер. Пока просто элемент массива

			_driver[dev]->AddPacket(data, length);
		}

		// Ручной вызов фиксации пакета L3PacketSecurity. Вызывается тогда, когда мы всё что хотели отправить - отправили и хотим выйти их функции, чтобы недобранный пакет финализировать и отправить.
		void Commit(uint8_t dev)
		{
			// Ищем по маске dev нужный драйвер. Пока просто элемент массива

			_driver[dev]->Commit();
		}

	private:
		
		Driver *_driver[8];
		uint8_t _driver_idx;
};


Manager manager;


structpack can_frame_t
{
	uint8_t fId = 0x17;		// Пакет изменения состояния устройства CAN
	uint16_t canId;			// ID устройства на шине CAN
	uint8_t data_len;		// CAN длина данных
	uint8_t data[8];		// CAN данные
};


void qwe()
{
	can_frame_t p1 = {};
	p1.canId = 0x0126;
	p1.data_len = 2;
	p1.data[0] = 0x65;
	p1.data[1] = 0xFF;
	
	can_frame_t p2 = {};
	p2.canId = 0x0127;
	p2.data_len = 2;
	p2.data[0] = 0x65;
	p2.data[1] = 0x00;

	manager.AddPacket(1, (uint8_t *)&p1, 6);
	manager.AddPacket(1, (uint8_t *)&p2, 6);
	manager.Commit(1);
}
