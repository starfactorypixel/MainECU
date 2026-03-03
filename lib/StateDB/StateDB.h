/*
	Класс базы данных полученных параметров из шины CAN.
	Пока настроен на версию CAN 2.0A. Реализация версии 2.0B потребует использовать другой подход, с динамическими списками :(
	В данный момент: (1 + 8 + 1 + 4) * 2048 = 26624 = 28КБ SRAM памяти занимает эта БД.
*/

#pragma once

#include <string.h>
#include <L3Constants.h>

class StateDB
{
	static constexpr uint16_t _max_id = 2048;	// Максимальный ID хранимый в БД, от 0 до (_max_id - 1).
	static constexpr uint8_t _max_data = 8;		// Максимальное кол-во байт в поле данных.
	
	public:
		
		struct __attribute__((packed)) db_t
		{
			uint8_t isset;					// Флаг наличия данных в ячейке.
			uint8_t length;					// Полезная длина данных.
			uint8_t data[_max_data];		// Байты данных, как в CAN пакете.
			uint32_t time;					// Время последнего изменения данных.
		};
		
		StateDB()
		{
			memset(&_db, 0x00, sizeof(_db));
			memset((void *)_pending_mask, 0x00, sizeof(_pending_mask));
			
			return;
		}
		
		bool Set(uint16_t id, uint8_t *data, uint8_t length, uint32_t time)
		{
			if(id >= _max_id) return false;
			if(length > _max_data) return false;
			
			db_t &db_obj = _db[id];
			db_obj.isset = 1U;
			db_obj.length = length;
			memcpy(db_obj.data, data, length);
			db_obj.time = time;
			_mark_updated(id);

			return true;
		}
		
		bool Set(uint16_t id, db_t &obj)
		{
			if(id >= _max_id) return false;
			
			memcpy(&_db[id], &obj, sizeof(db_t));
			_mark_updated(id);
			
			return true;
		}
		
		bool Get(uint16_t id, uint8_t *&data, uint8_t &length, uint32_t &time)
		{
			if(id >= _max_id) return false;
			
			db_t &obj = _db[id];
			if(obj.isset == 0U) return false;
			
			length = obj.length;
			data = obj.data;
			time = obj.time;
			
			return true;
		}

		bool Get(uint16_t id, db_t &obj)
		{
			if(id >= _max_id) return false;
			
			db_t &db_obj = _db[id];
			obj = db_obj;
			
			return (db_obj.isset == 1U);
		}
		
		bool Del(uint16_t id)
		{
			if(id >= _max_id) return false;
			
			memset(&_db[id], 0x00, sizeof(db_t));
			
			return true;
		}
		
		void Processing2(uint32_t &time, void (*func)(uint16_t can_id, db_t &db_obj))
		{
			//uint16_t idx = 0;
			//for(db_t &obj : _db)
			for(uint16_t idx = 0; idx < _max_id; ++idx)
			{
				db_t &obj = _db[idx];
				
				if(obj.isset == 1U) continue;
				
				func(idx, obj);
				
				//++idx;
			}
			
			return;
		}
		
		void Processing(void (*func)(uint16_t can_id, db_t &obj))
		{
			for(uint16_t word = 0; word < (sizeof(_pending_mask) / sizeof(_pending_mask[0])); ++word)
			{
				uint16_t bits = __atomic_exchange_n(&_pending_mask[word], 0, __ATOMIC_ACQUIRE);
				
				while(bits)
				{
					uint16_t bit = __builtin_ctz(bits);
					uint16_t idx = (word << 4) + bit;
					
					db_t &db_obj = _db[idx];
					
					if(db_obj.isset)
						func(idx, db_obj);
					
					bits &= ~(1U << bit);
				}
			}

			return;
		}
		
		void Dump(void (*func)(uint16_t id, db_t &obj), bool all = false)
		{
			for(uint16_t i = 0; i < _max_id; ++i)
			{
				if(all == true || _db[i].isset == 1U)
				{
					func(i, _db[i]);
				}
			}
			
			return;
		}
		
	private:
		
		inline void _mark_updated(uint16_t idx)
		{
			uint32_t word = idx >> 4;
			uint32_t mask = 1U << (idx & 0x0F);
			__atomic_fetch_or(&_pending_mask[word], mask, __ATOMIC_RELEASE);
		}
		
		db_t _db[_max_id];
		
		// Массив-маска обновлённых данных
		volatile uint16_t _pending_mask[_max_id / 16];

};
