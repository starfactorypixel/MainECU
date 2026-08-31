/*
	Обёртка пакета L2.
*/

#pragma once
#include <stdint.h>
#include <ESP32SJA1000.h>
#include <CUtils_RingBuffer.h>

class L2Wrapper
{
	static const uint8_t _rx_buff_size = 128U;
	
	public:
		//using packet_t = ESP32SJA1000Class::packet_t;
		using packet_t = ESP32SJA1000Class::packet_new_t;
		struct packet_v2_t
		{
			uint16_t id;
			uint8_t raw_data_len;
			union
			{
				uint8_t raw_data[8];
				struct
				{
					uint8_t func_id;
					uint8_t data[7];
				};
			};
		};

		using callback_event_t = bool (*)(packet_t &request, packet_t &response);
		using callback_error_t = void (*)(int8_t code);

		enum error_t : int8_t
		{
			ERR_NONE = 0,
			ERR_OVERFLOW = -1
		};

		L2Wrapper()
		{

		}
		
		void Init()
		{
			this->_driver.setPins(GPIO_NUM_5, GPIO_NUM_4);
			this->_driver.begin(500000);
			this->_driver.setCallback([&](packet_t &packet)
			{
				if( this->_request_buff.Write(packet) == false )
				{
					// Значит получили новый пакет, но буфер полный.. DragonPanic.
					this->_rx_overflow = true;
				}
			});

			
			//pinMode(GPIO_NUM_21, OUTPUT);
			//digitalWrite(GPIO_NUM_21, LOW);
			#warning CAN_RS ctrl by hc595
			
			return;
		}
		
		void RegCallback(callback_event_t event, callback_error_t error)
		{
			this->_callback_event = event;
			this->_callback_error = error;
			
			return;
		}
		
		bool Send(packet_t &packet)
		{
			#warning Delete this && Replace to 'bool Send(packet_v2_t &packet)' && Use orginal library;

#if defined(NO_CAN_SEND)
			return true;
#else
			return this->_driver.SendPacket(packet);
#endif
		}
		
		bool Send(packet_v2_t &packet)
		{
			bool result = false;
			
#if defined(NO_CAN_SEND)
			result = true;
#else
			this->_driver.beginPacket(packet.id);
			this->_driver.write(packet.raw_data, packet.raw_data_len);
			result = this->_driver.endPacket();
#endif
			
			return result;
		}
		
		bool Send(uint16_t id, uint8_t data[8], uint8_t length)
		{
			bool result = false;
			
#if defined(NO_CAN_SEND)
			result = true;
#else
			this->_driver.beginPacket(id);
			this->_driver.write(data, length);
			result = this->_driver.endPacket();
#endif
			
			return result;
		}
		
		void Processing(uint32_t &time)
		{
			struct { packet_t request; packet_t response; } packets;
			
			while(this->_request_buff.IsEmpty() == false)
			{
				if(this->_rx_overflow == true)
				{
					this->_rx_overflow = false;
					this->_callback_error( ERR_OVERFLOW );
				}
				
				memset(&packets, 0x00, sizeof(packets));
				
				this->_request_buff.Read(packets.request);
				if( this->_callback_event(packets.request, packets.response) == true )
				{
					this->_driver.SendPacket(packets.response);
				}
			}
			
			return;
		}

	private:
		ESP32SJA1000Class _driver;
		callback_event_t _callback_event;
		callback_error_t _callback_error;
		
		RingBuffer<_rx_buff_size, packet_t> _request_buff;
		bool _rx_overflow;
};
