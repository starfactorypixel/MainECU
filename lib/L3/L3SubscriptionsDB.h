/*
	Класс базы данных подписок устройств L3 на оповещения о изменении состояния контролируемых параметров.
	В данный момент: 1 * 2048 = 2048 = 2КБ SRAM памяти занимает эта БД.
*/

#pragma once

#include <string.h>
#include <L3Constants.h>

class L3SubscriptionsDB
{
	static const uint16_t _max_id = 2048;   // Максимальный ID хранимый в БД, от 0 до (_max_id - 1).
	
	public:
		
		L3SubscriptionsDB()
		{
			memset(&_db, 0x00, sizeof(_db));
			
			return;
		}
		
		/*
			Устанавливает отметку подписки устройства.
			> uint16_t id - ID параметра, от 0 до (_max_id - 1);
			> L3DevType_t dev - Тип устройства;
			> return - true в случае успеха;
		*/
		bool Set(uint16_t id, L3DevType_t dev, bool temporary = false)
		{
			bool result = false;
			
			if(id < _max_id)
			{
				if(temporary == true && (_db[id].devices & dev) == 0) _db[id].temporary |= dev;
				_db[id].devices |= dev;
				
				result = true;
			}
			
			return result;
		}
		
		/*
			Возвращает отметку подписки устройства. Не удаляет временную подписку.
			> uint16_t id - ID параметра, от 0 до (_max_id - 1);
			> L3DevType_t dev - Тип устройства;
			> return - true в случае успеха;
		*/
		bool Get(uint16_t id, L3DevType_t dev)
		{
			bool result = false;
			
			if(id < _max_id)
			{
				if( (_db[id].devices & dev) == dev )
				{
					result = true;
				}
			}
			
			return result;
		}
		
		/*
			Возвращает маску подписанных устройств. Удаляет временную подписку.
			> uint16_t id - ID параметра, от 0 до (_max_id - 1);
			> return - Маска устройств типа L3DevType_t;
		*/
		L3DevType_t GetDev(uint16_t id)
		{
			L3DevType_t result = L3_DEVTYPE_NONE;
			
			if(id < _max_id)
			{
				result = (L3DevType_t)_db[id].devices;
				Del(id, (L3DevType_t)_db[id].temporary);
			}
			
			return result;
		}
		
		/*
			Удаляет отметку подписки устройства.
			> uint16_t id - ID параметра, от 0 до (_max_id - 1);
			> L3DevType_t dev - Тип устройства;
			> return - true в случае успеха;
		*/
		void Del(uint16_t id, L3DevType_t dev)
		{
			if(id < _max_id)
			{
				_db[id].devices &= ~dev;
				_db[id].temporary &= ~dev;
			}
			
			return;
		}
		
		/*
			Удаляет отметку подписки устройства на все ID.
			> L3DevType_t dev - Тип устройства;
			> return - true в случае успеха;
		*/
		void DelDev(L3DevType_t dev)
		{
			for(db_t &element : _db)
			{
				element.devices &= ~dev;
				element.temporary &= ~dev;
			}
			
			return;
		}
		
	private:
		struct db_t
		{
			uint8_t devices;        // Маска устройств, подписанных на ID.
			uint8_t temporary;      // Маска временных подписок устройств.
		} _db[_max_id];
		
};
