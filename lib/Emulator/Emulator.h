/*
	
*/

#pragma once

class VirtualDeviceInterface
{
	public:
		virtual ~VirtualDeviceInterface() = default;
		
		virtual void GetValueBytes(uint8_t *bytes, uint8_t &length) const = 0;
		virtual bool UpdateValue(uint32_t current_time) = 0;
		virtual uint32_t GetID() const = 0;
};

template <typename T>
class VirtualDevice : public VirtualDeviceInterface
{
	public:
		enum algorithm_t
		{
			ALG_NONE,			// Статическое значение.
			ALG_RANDOM,			// Случайное значение в пределах диапазона.
			ALG_MINMAX,			// Триггерное переключение между min и max.
			ALG_MINFADEMAX		// Плавное перемещение между min и max.
		};
		
		VirtualDevice(uint32_t id, T min, T max, uint16_t interval, T step, T value, algorithm_t algorithm) : config{id, min, max, interval, step, value, algorithm}
		{
			return;
		}
		
		~VirtualDevice() = default;
		
		void GetValueBytes(uint8_t *bytes, uint8_t &length) const
		{
			memcpy(bytes, &config.value, sizeof(T));
			length = sizeof(T);
			
			/*
			// Перфекционизм или дебилизм?
			for(uint8_t i = 0; i < (length / 2); ++i)
			{
				uint8_t t = bytes[i];
				bytes[i] = bytes[length - i - 1];
				bytes[length - i - 1] = t;
			}
			*/
			
			return;
		}
		
		bool UpdateValue(uint32_t current_time)
		{
			bool result = false;
			
			if(current_time - config.update > config.interval)
			{
				config.update = current_time;
				
				switch(config.algorithm)
				{
					case ALG_RANDOM:
					{
						// Некорректно работает с float.
						config.value = random(config.min, config.max);
						
						break;
					}
					case ALG_MINMAX:
					{
						if(config.value == config.min)
							config.value = config.max;
						else
							config.value = config.min;
						
						break;
					}
					case ALG_MINFADEMAX:
					{
						T val_to = (config.direction) ? config.max : config.min;
						if( abs((long)(config.value - val_to)) / config.step > 0 )
						{
							if(config.value > val_to){ config.value -= config.step; }
							else{ config.value += config.step; }
						}
						else
						{
							config.value = val_to;
							config.direction = !config.direction;
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
		
		uint32_t GetID() const
		{
			return config.id;
		}
		
	private:
		
		struct config_t
		{
			uint32_t id;			// Идентификатор датчика.
			T min;					// Минимальное значение датчика.
			T max;					// Максимальное значение датчика.
			uint16_t interval;		// Интервал обновления значения датчика.
			T step;					// Шаг изменения значения датчика за указанный интервал.
			T value;				// Текущее значение датчика.
			algorithm_t algorithm;	// Алгоритм обновления значение датчика.
			uint32_t update;		// Время последнего обновления значения датчика.
			bool direction;			// Направление изменения значения датчика: true - вверх, false - вниз.
		} config;
};

class Emulator
{
	public:
		
		using callback_event_t = void (*)(uint32_t id, uint8_t *bytes, uint8_t length, uint32_t time);
		
		Emulator(callback_event_t callback = nullptr)
		{
			this->_callback = callback;
			
			return;
		}
		
		void RegDevice(VirtualDeviceInterface &obj)
		{
			this->_data[_obj_idx].id = obj.GetID();
			this->_data[_obj_idx].obj = &obj;
			this->_obj_idx++;
			
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
			
			for(uint8_t i = 0; i < this->_obj_idx; ++i)
			{
				if( this->_data[i].id == id )
				{
					this->_data[i].obj->GetValueBytes(bytes, length);
					
					result = true;
					
					break;
				}
			}
			
			return result;
		}
		
		void Processing(uint32_t time)
		{
			if(time - this->_ticktime > 5)
			{
				for(uint8_t i = 0; i < this->_obj_idx; ++i)
				{
					if( this->_data[i].obj->UpdateValue(time) == true )
					{
						uint8_t bytes[8];
						uint8_t length;
						this->_data[i].obj->GetValueBytes(bytes, length);
						
						if(this->_callback != nullptr)
							this->_callback(this->_data[i].id, bytes, length, time);
					}
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
		} _data[64];
		uint8_t _obj_idx = 0;
		uint32_t _ticktime = 0;
};
