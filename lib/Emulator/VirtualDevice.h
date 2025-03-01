#pragma once
#include <inttypes.h>
#include "VirtualDeviceInterface.h"

template <typename T> 
class VirtualDevice : public VirtualDeviceInterface
{
	public:
		
		VirtualDevice(uint32_t id, uint8_t fId, T min, T max, uint16_t interval, T step, T value, algorithm_t algorithm) : _config{id, fId, min, max, interval, step, value, algorithm, 0, false}
		{
			return;
		}
		
		~VirtualDevice() = default;
		
		
		virtual void GetValueBytes(uint8_t *bytes, uint8_t &length) const override
		{
			bytes[0] = _config.fId;
			memcpy(&bytes[1], &_config.value, sizeof(T));
			length = sizeof(T) + 1;
			
			return;
		}
		
		virtual bool UpdateValue(uint32_t current_time) override
		{
			bool result = false;
			
			if(current_time - _config.update > _config.interval)
			{
				_config.update = current_time;
				
				switch(_config.algorithm)
				{
					case ALG_RANDOM:
					{
						// Некорректно работает с float.
						_config.value = random(_config.min, _config.max);
						
						break;
					}
					case ALG_MINMAX:
					{
						if(_config.value == _config.min)
							_config.value = _config.max;
						else
							_config.value = _config.min;
						
						break;
					}
					case ALG_MINFADEMAX:
					{
						T val_to = (_config.direction) ? _config.max : _config.min;
						if( abs((long)(_config.value - val_to)) / _config.step > 0 )
						{
							if(_config.value > val_to){ _config.value -= _config.step; }
							else{ _config.value += _config.step; }
						}
						else
						{
							_config.value = val_to;
							_config.direction = !_config.direction;
						}
					}
					default:
					{
						break;
					}
				}
				
				result = true;
			}
			
			return result;
		}
		
		virtual uint32_t GetID() const override
		{
			return _config.id;
		}
		
	private:
		
		struct config_t
		{
			uint32_t id;			// Идентификатор датчика
			uint8_t fId;			// Тип Function ID для ответа
			T min;					// Минимальное значение датчика
			T max;					// Максимальное значение датчика
			uint16_t interval;		// Интервал обновления значения датчика
			T step;					// Шаг изменения значения датчика за указанный интервал
			T value;				// Текущее значение датчика
			algorithm_t algorithm;	// Алгоритм обновления значение датчика
			uint32_t update;		// Время последнего обновления значения датчика
			bool direction;			// Направление изменения значения датчика: true - вверх, false - вниз
		} _config;
		
};
