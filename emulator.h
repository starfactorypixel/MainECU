#pragma once


template <typename T>
class VirtualDevice
{
	public:
		VirtualDevice(uint32_t id, T min, T max, T interval, T value) : config{id, min, max, interval, value}
		{
			return;
		}
		
		enum algorithm_t {ALG_NONE, ALG_RANDOM, ALG_MINMAX};
		
		struct config_t
		{
			uint32_t id;			// Идентификатор датчика.
			T min;					// Минимальное значение датчика.
			T max;					// Максимальное значение датчика.
			T interval;				// Интервал обновления значения датчика.
			T value;				// Текущее значение датчика.
			algorithm_t algorithm;	// Алгоритм обновления значение датчика.
			uint32_t update;		// Время последнего обновления значения датчика.
		} config;
};

class Emulator
{
	public:
		template <class C>
		void RegDevice(C &obj)
		{
			this->_obj[_obj_idx++] = &obj;
			
			auto val = (&obj)->config.value;
			Serial.print("val = ");
			Serial.println( val );
		}
		
		void Processing(uint32_t time = millis())
		{
			if(time - this->_ticktime > 5)
			{
				this->_ticktime = time;
				
				void *obj = nullptr;
				for(uint8_t i = 0; i < this->_obj_idx; ++i)
				{
					Serial.print("i = ");
					Serial.print(i);
					Serial.print(", val = ");
					//Serial.println( this->_obj[i]->config.value );
					Serial.println( );
					
					obj = this->_obj[i].config;
					if( time - obj.update > obj.interval )
					{
						obj.update = time;
						
						switch(obj.algorithm)
						{
							case ALG_RANDOM:
							{
								obj.value = random(obj.min, obj.max);
								
								break;
							}
							case ALG_MINMAX:
							{
								break;
							}
							
							default:
							{
								break;
							}
						}
						
					}
					
					
					
					
					
					
					
				}
			}
			
			return;
		}
		
		
	
	private:
		
		void *_obj[64];
		uint8_t _obj_idx = 0;
		
		uint32_t _ticktime = 0;
};
