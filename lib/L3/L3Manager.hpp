#pragma once
#include <inttypes.h>
#include "L3ManagerInterface.hpp"
#include "Drivers/L3Driver.hpp"

class L3Manager : public L3ManagerInterface
{
	using get_time_ms_function_t = uint32_t (*)();
	using rx_packet_function_t = uint16_t (*)(L3Driver *driver, const uint8_t *rx, uint16_t rx_len);
	
	public:
	
		L3Manager() = delete;
		L3Manager(const L3Manager &) = delete;
		L3Manager &operator=(const L3Manager &) = delete;
		L3Manager(rx_packet_function_t rx, get_time_ms_function_t millis) : _RxCallback(rx), _Millis(millis)
		{

		}

		void AddDriver(L3Driver &driver)
		{
			driver.Init(this);
			uint8_t type = driver.GetType();
			_drivers[__builtin_ctz(type)] = &driver;
			
			return;
		}
		
		L3Driver *GetDriver(uint8_t dev)
		{
			return _drivers[__builtin_ctz(dev)];
		}

		// Метод вызывается когда пришёл готовый пакет.
		virtual uint16_t RxPacket(uint8_t dev_type, const uint8_t *rx, uint16_t rx_len) override
		{
			// Пришёл пакет
			// В rx_len указана максимальная длина которую можно обработать. это НЕ длина пакета, это >= длина пакета. Но хорошо бы убедиться что это ещё и не < пакета.
			
			uint8_t fId = rx[0];
			// Работа с rx
			L3Driver *driver = GetDriver(dev_type);
			_RxCallback(driver, rx, rx_len);

			// В конце необходимо вернуть фактическую длину пакета, которую вычислили базе fId.
			return 6;
		}

		// Добавляем программный пакет на отправку
		void AddPacket(uint8_t dev, const uint8_t *data, uint16_t length)
		{
			while(dev)
			{
				uint8_t bit = dev & -dev;
				_drivers[bit]->AddPacket(data, length);
				dev &= ~bit;
			}
			
			return;
		}

		// Ручной вызов фиксации пакета L3PacketSecurity. Вызывается тогда, когда мы всё что хотели отправить - отправили и хотим выйти их функции, чтобы недобранный пакет финализировать и отправить.
		void Commit(uint8_t dev)
		{
			while(dev)
			{
				uint8_t bit = dev & -dev;
				_drivers[bit]->Commit();
				dev &= ~bit;
			}
			
			return;
		}

		// Сброс устройства
		void Reset(uint8_t dev)
		{
			while(dev)
			{
				uint8_t bit = dev & -dev;
				_drivers[bit]->Reset();
				dev &= ~bit;
			}
			
			return;
		}

		void Processing()
		{
			for(auto &driver : _drivers)
			{
				if(driver == nullptr) continue;
				
				uint32_t time = _Millis();
				driver->Processing(time);
				driver->Commit();					// Спорно, но гарантирует что всё что напихали в TX будет отправлено когда конкретный драйвер закончит обработку.
			}
			
		}

	private:
		
		rx_packet_function_t _RxCallback;
		get_time_ms_function_t _Millis;

		L3Driver *_drivers[8];
};
