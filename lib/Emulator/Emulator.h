#pragma once
#include <inttypes.h>
#include "VirtualDevice.h"

template <uint16_t _count> 
class Emulator
{
	static constexpr uint32_t TICK_TIME = 6;
	
	using callback_event_t = void (*)(uint32_t id, uint8_t *bytes, uint8_t length, uint32_t time);
	
	public:
		
		Emulator(callback_event_t callback) : _callback(callback)
		{
			memset(_data, 0x00, sizeof(_data));
			
			return;
		}
		
		void RegDevice(VirtualDeviceInterface &obj)
		{
			if(_obj_idx >= _count) return;
			
			_data[_obj_idx].id = obj.GetID();
			_data[_obj_idx].obj = &obj;
			_obj_idx++;
			
			return;
		}
		
		/*
			Запрашивает значение конкретного датчика.
				id - ID датчика.
				bytes - Массив возвращаемых байт, представляющие значение датчика в порядке Little Endian DCBA
				length - Длина массива выше.
				return - true если ID датчика найдено, false - если нет.
		*/
		bool Request(uint32_t id, uint8_t *bytes, uint8_t &length)
		{
			bool result = false;
			
			for(uint8_t i = 0; i < _obj_idx; ++i)
			{
				if(_data[i].id != id) continue;
				
				_data[i].obj->GetValueBytes(bytes, length);
				result = true;
				
				break;
			}
			
			return result;
		}
		
		void Processing(uint32_t &time)
		{
			if(time - _lasttick > TICK_TIME)
			{
				_lasttick = time;
				
				uint8_t bytes[8] = {};
				uint8_t length = 0;
				data_t *data = nullptr;
				for(uint8_t i = 0; i < _obj_idx; ++i)
				{
					data = &_data[i];
					
					if( data->obj->UpdateValue(time) == false ) continue;
					
					data->obj->GetValueBytes(bytes, length);
					_callback(data->id, bytes, length, time);
				}
			}
			
			return;
		}
		
	private:
		
		callback_event_t _callback;
		
		struct data_t
		{
			uint32_t id;
			VirtualDeviceInterface *obj;
		} _data[_count];
		uint8_t _obj_idx = 0;
		uint32_t _lasttick = 0;
};
