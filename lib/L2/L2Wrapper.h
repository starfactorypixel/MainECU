/*
	Обёртка пакета L2.
*/

#pragma once

#include <stdint.h>
#include <ESP32SJA1000.h>
#include <RingBuffer.h>

class L2Wrapper
{
	static const uint8_t _rx_buff_size = 16;
	
	public:
		
		using callback_event_t = bool (*)(ESP32SJA1000Class::packet_t &request, ESP32SJA1000Class::packet_t &response);
		using callback_error_t = void (*)(int8_t code);



		
		void Init()
		{
			this->_driver.setPins(GPIO_NUM_4, GPIO_NUM_5);
			this->_driver.onReceive([&]( ESP32SJA1000Class::packet_t packet )
			{
				if( this->_request_buff.Write( packet ) == false )
				{
					// Значит получили новый пакет, но буфер полный.. DragonPanic.
					this->_rx_overflow = true;
				}
			});
			this->_driver.begin(500000);
			
			return;
		}
		
		void RegCallback(callback_event_t event, callback_error_t error)
		{
			this->_callback_event = event;
			this->_callback_error = error;
			
			return;
		}
		
		bool Send(ESP32SJA1000Class::packet_t packet)
		{
            #warning Implementation required!

			return true;
		}
		
		void Processing(uint32_t time)
		{
			if(this->_request_buff.IsEmpty() == false)
			{
				if(this->_rx_overflow == true)
				{
					this->_rx_overflow = false;
					this->_callback_error(1);
				}
				
				ESP32SJA1000Class::packet_t _request;
				ESP32SJA1000Class::packet_t _response;

				this->_request_buff.Read(_request);
				if( this->_callback_event(_request, _response) == true )
				{
					this->_Send();
				}
			}
			
			return;
		}

	private:
		bool _Send()
		{
            #warning Implementation required!
            
			return true;
		}



		
		
		ESP32SJA1000Class _driver;
		callback_event_t _callback_event;
		callback_error_t _callback_error;




		RingBuffer<16, ESP32SJA1000Class::packet_t> _request_buff;

		bool _rx_overflow;


};
