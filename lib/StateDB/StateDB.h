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
			uint8_t isset:1;				// Флаг наличия данных в ячейке.
			uint8_t update:1;				// Флаг обновлённых, но не отправленных ( метод Processing() ) данных.
			uint8_t type:4;					// ID типа CAN объекта.
			uint8_t offset:2;				// offset.
			uint8_t data[_max_data];		// Байты данных, как в CAN пакете.
			uint8_t length;					// Полезная длина данных.
			uint32_t time;					// Время последнего изменения данных.
		};
		
		StateDB()
		{
			memset(&_db, 0x00, sizeof(_db));
			
			return;
		}
		
		bool Set(uint16_t id, uint8_t *data, uint8_t length, uint32_t time)
		{
			if(id >= _max_id) return false;
			if(length > _max_data) return false;
			
			db_t &db_obj = _db[id];
			db_obj.isset = 0b1;
			memcpy(db_obj.data, data, length);
			db_obj.length = length;
			db_obj.time = time;
			db_obj.update = 0b1;
			
			return true;
		}
		
		bool Set(uint16_t id, db_t &obj)
		{
			if(id >= _max_id) return false;
			
			memcpy(&_db[id], &obj, sizeof(db_t));
			
			return true;
		}
		
		void SetObjType(uint16_t id, uint8_t type)
		{
			if(id >= _max_id) return;
			
			_db[id].type = type;
			
			return;
		}
		
		bool Get(uint16_t id, uint8_t *&data, uint8_t &length, uint32_t &time)
		{
			if(id >= _max_id) return false;
			if(_db[id].isset == 0b0) return false;
			
			db_t &obj = _db[id];
			data = obj.data;
			length = obj.length;
			time = obj.time;
			
			return true;
		}
		
		bool Get(uint16_t id, db_t &obj)
		{
			if(id >= _max_id) return false;
			
			db_t &db_obj = _db[id];
			obj = db_obj;
			
			return (db_obj.isset == 0b1);
		}
		
		uint8_t GetObjType(uint16_t id)
		{
			if(id >= _max_id) return 0;
			
			return _db[id].type;
		}
		
		bool Del(uint16_t id)
		{
			if(id >= _max_id) return false;
			
			memset(&_db[id], 0x00, sizeof(db_t));
			
			return true;
		}
		
		void Processing(uint32_t &time, void (*func)(uint16_t can_id, db_t &db_obj))
		{
			//uint16_t idx = 0;
			//for(db_t &obj : _db)
			for(uint16_t idx = 0; idx < _max_id; ++idx)
			{
				db_t &obj = _db[idx];
				
				if(obj.isset == 0b0) continue;
				if(obj.update == 0b0) continue;
				if(obj.type == 0) continue;
				
				func(idx, obj);
				
				obj.update = 0b0;
				//++idx;
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
