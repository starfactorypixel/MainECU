#pragma once
#include <inttypes.h>
#include <string.h>
#include "L2Consts.hpp"
#include "Drivers/L2DriverInterface.hpp"

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

			void (*error)(uint32_t errors, const L2Statistics &stats);	// errors — флаги L2_ERR_*
		};
		
		L2Manager() = delete;
		L2Manager(const L2Manager &) = delete;
		L2Manager &operator=(const L2Manager &) = delete;
		L2Manager(const callbacks_t &callbacks, get_time_ms_function_t millis) : _callbacks(callbacks), _Millis(millis), _driver(nullptr)
		{
			return;
		}
		
		bool SetDriver(L2DriverInterface &driver)
		{
			if(_driver != nullptr)
				_driver->End();
			
			_driver = &driver;
			return _driver->Begin();
		}
		
		void Processing()
		{
			if(_driver == nullptr) return;
			
			// Ошибки драйвера + ошибки, отложенные в Send()
			uint32_t errors = _driver->Poll();
			errors |= __atomic_exchange_n(&_pending_errors, 0U, __ATOMIC_RELAXED);
			
			L2Frame frame = {};
			for(uint16_t i = 0; i < RX_PER_TICK; ++i)
			{
				if(_driver->Receive(frame) == false)
					break;
				
				_stats.rx_count++;
				if(_callbacks.rx != nullptr)
					_callbacks.rx(frame, _Millis());
			}
			
			// После приёма — чтобы в снимке были актуальные счётчики
			if(errors != L2_ERR_NONE && _callbacks.error != nullptr)
			{
				L2Statistics stats = GetStatistics();
				_callbacks.error(errors, stats);
			}
			
			return;
		}
		
		bool Send(const L2Frame &frame)
		{
#ifndef NO_CAN_SEND
			if(_driver == nullptr || _driver->Transmit(frame) == false)
			{
				_stats.tx_reject_count++;
				__atomic_fetch_or(&_pending_errors, L2_ERR_TX_REJECT, __ATOMIC_RELAXED);
				return false;
			}
#endif
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
		
		L2Statistics GetStatistics() const
		{
			L2Statistics stats = _stats;
			if(_driver != nullptr)
				_driver->GetStatistics(stats);
			
			return stats;
		}
		
	private:
		
		callbacks_t _callbacks;
		get_time_ms_function_t _Millis;
		L2DriverInterface *_driver;
		
		L2Statistics _stats = {};
		uint32_t _pending_errors = 0;		// Флаги L2_ERR_*, выставленные вне Processing(); доступ только через __atomic_*
};
