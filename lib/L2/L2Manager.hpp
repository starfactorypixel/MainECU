#pragma once
#include <inttypes.h>
#include <string.h>
#include "L2Consts.hpp"
#include "Drivers/L2DriverInterface.hpp"

// Единая точка работы с CAN для остального кода. О конкретном драйвере ничего не знает.
// Tick() вызывается из loop: обслуживает драйвер и разбирает принятые кадры, вызывая колбек на каждый.
// Send() можно вызывать откуда угодно (кроме ISR), если это допускает драйвер; вызов сразу возвращается.

class L2Manager
{
	// Ограничение на число кадров за один Tick(), чтобы при шторме на шине loop не застревал навсегда.
	// Остаток будет разобран на следующем Tick(). Должно быть не меньше длины очереди приёма драйвера.
	static constexpr uint16_t RX_PER_TICK = 256;

	using get_time_ms_function_t = uint32_t (*)();

	public:

		struct callbacks_t
		{
			void (*rx)(const L2Frame &frame, uint32_t time);
		};

		L2Manager() = delete;
		L2Manager(const L2Manager &) = delete;
		L2Manager &operator=(const L2Manager &) = delete;
		L2Manager(const callbacks_t &callbacks, get_time_ms_function_t millis) : _callbacks(callbacks), _Millis(millis), _driver(nullptr)
		{
			return;
		}

		void SetDriver(L2DriverInterface &driver)
		{
			_driver = &driver;
			_driver->Init(_stats);
			_driver->Begin();

			return;
		}

		void Tick()
		{
			if(_driver == nullptr) return;

			_driver->Poll();

			L2Frame frame = {};
			for(uint16_t i = 0; i < RX_PER_TICK; ++i)
			{
				if(_driver->Receive(frame) == false) break;

				uint32_t time = _Millis();

				_stats.rx_count++;
				if(_callbacks.rx != nullptr)
					_callbacks.rx(frame, time);
			}

			return;
		}

		bool Send(const L2Frame &frame)
		{
			if(_driver == nullptr || _driver->Transmit(frame) == false)
			{
				_stats.tx_drop_count++;
				return false;
			}

			_stats.tx_count++;

			return true;
		}

		bool Send(uint32_t id, const uint8_t *data, uint8_t length, bool extended = false)
		{
			L2Frame frame;
			frame.id = id;
			frame.extended = extended;
			frame.rtr = false;
			frame.length = (length > 8) ? 8 : length;
			if(frame.length > 0 && data != nullptr)
				memcpy(frame.data, data, frame.length);

			return Send(frame);
		}

		bool IsOnline() const
		{
			return (_driver != nullptr && _driver->IsRunning());
		}
		
		L2Statistics &GetStatistics()
		{
			return _stats;
		}

	private:
		
		callbacks_t _callbacks;
		get_time_ms_function_t _Millis;
		L2DriverInterface *_driver;

		L2Statistics _stats = {};
};
