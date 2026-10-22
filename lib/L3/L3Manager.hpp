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
		L3Manager(rx_packet_function_t rx, get_time_ms_function_t millis) : _RxCallback(rx), _Millis(millis), _drivers{}
		{

		}

		void AddDriver(L3Driver &driver)
		{
			driver.Attach(this);
			driver.Open();
			
			uint8_t type = driver.GetType();
			_drivers[__builtin_ctz(type)] = &driver;
			
			return;
		}
		
		// dev — один бит маски (тип драйвера). Хранение по номеру бита: DASHBOARD (0x01) -> [0], SERVER (0x80) -> [7]
		L3Driver *GetDriver(uint8_t dev)
		{
			if(dev == 0) return nullptr;	// __builtin_ctz(0) не определён

			return _drivers[__builtin_ctz(dev)];
		}

		// Метод вызывается когда пришёл готовый пакет.
		virtual uint16_t RxPacket(L3Driver *driver, const uint8_t *rx, uint16_t rx_len) override
		{
			// Пришёл пакет
			// В rx_len указана максимальная длина которую можно обработать. это НЕ длина пакета, это >= длина пакета. Но хорошо бы убедиться что это ещё и не < пакета.
			

			// Колбэк обязан вернуть фактическую длину сообщения, вычисленную по fId (0 — остаток не разбирать)
			return _RxCallback(driver, rx, rx_len);
		}

		// Добавляем программный пакет на отправку
		void AddPacket(uint8_t dev, const uint8_t *data, uint16_t length)
		{
			while(dev)
			{
				uint8_t bit = dev & -dev;
				L3Driver *driver = GetDriver(bit);	// bit — значение бита, не индекс
				if(driver != nullptr) driver->AddPacket(data, length);
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
				L3Driver *driver = GetDriver(bit);	// bit — значение бита, не индекс
				if(driver != nullptr) driver->Commit();
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
				L3Driver *driver = GetDriver(bit);	// bit — значение бита, не индекс
				if(driver != nullptr) driver->Reset();
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
