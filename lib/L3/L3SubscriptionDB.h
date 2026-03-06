/*
	Класс базы данных подписок устройств L3 на оповещения о изменении состояния контролируемых параметров.
	В данный момент: 2 * 2048 = 4096 = 4КБ SRAM памяти занимает эта БД.
*/

#pragma once
#include <inttypes.h>
#include <L3Constants.h>

class L3SubscriptionDB
{
	static constexpr uint16_t _max_id = 2048;	// Максимальный ID хранимый в БД, от 0 до (_max_id - 1).
	
	typedef uint8_t dev_mask_t;
	
	public:
		
		L3SubscriptionDB() : _db{}
		{}
		
		inline void Subscribe(uint16_t id, dev_mask_t dev, bool temp)
		{
			if(id >= _max_id) return;

			if(temp == false)
				_db[id].persistent |= dev;
			else
				_db[id].temporary |= dev;
			
			return;
		}
		
		inline void Unsubscribe(uint16_t id, dev_mask_t dev)
		{
			if(id >= _max_id) return;
			
			_db[id].persistent &= ~dev;
			_db[id].temporary &= ~dev;
			
			return;
		}
		
		inline void Unsubscribe(dev_mask_t dev)
		{
			for(db_t &el : _db)
			{
				el.persistent &= ~dev;
				el.temporary &= ~dev;
			}
			
			return;
		}
		
		inline void ClearTemp(uint16_t id, dev_mask_t dev)
		{
			if(id >= _max_id) return;
			
			_db[id].temporary &= ~dev;

			return;
		}
		
		inline bool CheckSubscribe(uint16_t id, dev_mask_t dev)
		{
			if(id >= _max_id) return false;

			return ((_db[id].persistent | _db[id].temporary) & dev) != 0;
		}
		
		inline uint8_t GetDevices(uint16_t id)
		{
			if(id >= _max_id) return 0;
			
			return _db[id].persistent | _db[id].temporary;
		}
		
		void Dump(dev_mask_t dev, void (*func)(uint16_t id))
		{
			for(uint16_t id = 0; id < _max_id; ++id)
			{
				if((_db[id].persistent | _db[id].temporary) & dev)
				{
					func(id);
				}
			}
			
			return;
		}
		
	private:
		
		struct db_t
		{
			uint8_t persistent;					// Маска постоянных подписок
			uint8_t temporary;					// Маска временных подписок
		} _db[_max_id];
		
};
