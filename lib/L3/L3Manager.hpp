#pragma once
#include <inttypes.h>
#include "L3ManagerInterface.hpp"
#include "Drivers/L3Driver.hpp"

class L3Manager : public L3ManagerInterface
{
	using get_time_ms_function_t = uint32_t (*)();

	public:

		// Колбэки приложения. Вызываются только в runtime: из Processing(), а error ещё из AddPacket()/Commit()
		// (ошибка отправки) и AddDriver() (ошибка запуска). Из задачи приёма — никогда. Любой может быть nullptr
		struct callbacks_t
		{
			// Сеанс открыт: первый валидный пакет клиента
			void (*open)(L3Driver *driver);

			// Сеанс закрыт: клиент замолчал (CLOSE_TIMEOUT) или транспорт сообщил об обрыве (CLOSE_TRANSPORT)
			void (*close)(L3Driver *driver, L3DriverInterface::close_reason_t reason);

			// Сообщение приложения (fId не 0x00/0x01). rx_len — сколько осталось в DATA, не длина сообщения.
			// Вернуть фактическую длину сообщения по fId, 0 — остаток не разбирать
			uint16_t (*rx)(L3Driver *driver, const uint8_t *rx, uint16_t rx_len);

			// Ошибка канала: отклонённый кадр (L3C::ERROR_RX_*), ошибка отправки (ERROR_TX_*), запуска (ERROR_SEQ_*)
			void (*error)(L3Driver *driver, L3C::error_t error);
		};
	
		L3Manager() = delete;
		L3Manager(const L3Manager &) = delete;
		L3Manager &operator=(const L3Manager &) = delete;
		L3Manager(const callbacks_t &callbacks, get_time_ms_function_t millis) : _callbacks(callbacks), _Millis(millis), _drivers{}
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

		// Транспортное сообщение fId разобрано драйвером, сюда приходят только сообщения приложения
		virtual uint16_t RxPacket(L3Driver *driver, const uint8_t *rx, uint16_t rx_len) override
		{
			if(_callbacks.rx == nullptr) return 0;

			return _callbacks.rx(driver, rx, rx_len);
		}

		virtual void SessionOpen(L3Driver *driver) override
		{
			if(_callbacks.open != nullptr) _callbacks.open(driver);
		}

		virtual void SessionClose(L3Driver *driver, L3DriverInterface::close_reason_t reason) override
		{
			if(_callbacks.close != nullptr) _callbacks.close(driver, reason);
		}

		virtual void Error(L3Driver *driver, L3C::error_t error) override
		{
			if(_callbacks.error != nullptr) _callbacks.error(driver, error);
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
		
		callbacks_t _callbacks;
		get_time_ms_function_t _Millis;

		L3Driver *_drivers[8];
};
