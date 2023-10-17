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
	static const uint16_t _max_id = 2048;	// Максимальный ID хранимый в БД, от 0 до (_max_id - 1).
	static const uint8_t _max_data = 8;		// Максимальное кол-во байт в поле данных.
	
	public:
		
		#pragma pack(push, 1)
		struct db_t
		{
			uint8_t isset:1;				// Флаг наличия данных в ячейке.
			uint8_t update:1;				// Флаг обновлённых, но не отправленных ( метод Processing() ) данных.
			uint8_t type:4;					// ID типа CAN объекта.
			uint8_t offset:2;				// offset.
			uint8_t data[_max_data];		// Байты данных, как в CAN пакете.
			uint8_t length;					// Полезная длина данных.
			uint32_t time;					// Время последнего изменения данных.
		};
		#pragma pack(pop)
		
		StateDB()
		{
			memset(&_db, 0x00, sizeof(_db));
			
			return;
		}
		
		bool Set(uint16_t id, uint8_t *data, uint8_t length, uint32_t time)
		{
			bool result = false;
			
			if(id < _max_id && length <= _max_data)
			{
				_db[id].isset = 0b1;
				_db[id].update = 0b1;
				memcpy(_db[id].data, data, length);
				_db[id].length = length;
				_db[id].time = time;

				result = true;
			}
			
			return result;
		}
		
		bool Set(uint16_t id, db_t &obj)
		{
			bool result = false;
			
			if(id < _max_id && obj.length <= _max_data)
			{
				_db[id].isset = obj.isset;
				_db[id].update = obj.update;
				memcpy(_db[id].data, obj.data, obj.length);
				_db[id].length = obj.length;
				_db[id].time = obj.time;
				#warning Replace to memcpy ?
				
				result = true;
			}
			
			return result;
		}

		void SetObjType(uint16_t id, uint8_t type)
		{
			if(id >= _max_id) return;
			
			_db[id].type = type;
			
			return;
		}
		
		bool Get(uint16_t id, uint8_t *&data, uint8_t &length, uint32_t &time)
		{
			bool result = false;

			if(id < _max_id && _db[id].isset == 0b1)
			{
				data = &_db[id].data[0];
				length = _db[id].length;
				time = _db[id].time;

				result = true;
			}
			else
			{
				data = 0x00;
				length = 0;
			}
			
			return result;
		}

		bool Get(uint16_t id, db_t &obj)
		{
			bool result = false;
			
			if(id < _max_id/* && _db[id].isset == 0b1*/)
			{
				obj = _db[id];
				
				result = true;
			}
			
			return result;
		}

		uint8_t GetObjType(uint16_t id)
		{
			if(id >= _max_id) return 0;
			
			return _db[id].type;
		}
		
		bool Del(uint16_t id, bool force = false)
		{
			bool result = false;
			
			if(id < _max_id)
			{
				_db[id].isset = 0b0;
				_db[id].update = 0b0;
				if(force == true)
				{
					memset(&_db[id], 0x00, sizeof(db_t));
				}
				
				result = true;
			}
			
			return result;
		}
		
		void Processing(uint32_t &time, void (*func)(uint16_t can_id, db_t &db_obj))
		{
			uint16_t idx = 0;
			for(db_t &obj : _db)
			{
				++idx;
				
				if(obj.isset == 0b0) continue;
				if(obj.update == 0b0) continue;
				if(obj.type == 0) continue;
				
				func(idx-1U, obj);
				obj.update = 0b0;
			}
			
			return;
		}
		
		void Dump(void (*func)(uint16_t id, db_t &obj), bool all = false)
		{
			for(uint16_t i = 0; i < _max_id; ++i)
			{
				if(all == true || _db[i].isset == 0b1)
				{
					func(i, _db[i]);
				}
			}
			
			return;
		}
		
	private:
		db_t _db[_max_id];

};
