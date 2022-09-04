/*
	Обёртка пакета L3.
*/

#pragma once

#include <L3Packet.h>
#include <L3Driver.h>

class L3Wrapper
{
	public:
		using packet_t = L3Packet<64>;
		using callback_event_t = bool (*)(packet_t &request, packet_t &response);
		using callback_error_t = void (*)(packet_t &request, int8_t code);
		
		L3Wrapper(uint8_t transport, L3Driver &driver) : _driver(&driver)
		{
			this->_transport = transport;
			
			switch(transport)
			{
				case 0x00: { _rx_packet.SetTimeout(25); break; };
				case 0x01: { _rx_packet.SetTimeout(5);  break; };
				case 0x02: { _rx_packet.SetTimeout(25); break; };
				case 0x03: { _rx_packet.SetTimeout(10); break; };
			}
			
			return;
		}
		
		void Init()
		{
			this->_driver->Init();

			return;
		}
		
		void RegCallback(callback_event_t event, callback_error_t error = nullptr)
		{
			this->_callback_event = event;
			this->_callback_error = error;

			return;
		}
		
		void SetUrgent()
		{
			this->_urgent_data = true;
			
			return;
		}
		
		// В будущем будет заменён на прерывание приёма байта.
		void IncomingByte()
		{
			if( this->_driver->ReadAvailable() > 0 )
			{
				byte incomingByte = this->_driver->ReadByte();
				
				if( _rx_packet.PutPacketByte(incomingByte, millis()) == true )	// УБРАТЬ millis !!!!
				{
					if( _rx_packet.IsReceived() == true )
					{
						if( this->_callback_event(_rx_packet, _tx_packet) == true )
						{
							// В колбеке необходимо установить как минимум два параметра: Type и Param.
							// Данные тоже, если нужно что-то передать.
							this->_Send();
						}
						_rx_packet.Init();
					}
				}
				
				if(_rx_packet.GetError() < 0)
				{
					this->_callback_error(_rx_packet, _rx_packet.GetError());
					_rx_packet.Init();
					
					// Или метод FlushBuffer() ?
					while(this->_driver->ReadAvailable() > 0){ this->_driver->ReadByte(); }
				}
			}
			
			return;
		}
		
		void Send(uint8_t type, uint16_t param, byte *data, uint8_t length)
		{
			_tx_packet.Type(type);
			_tx_packet.Param(param);
			for(int8_t i = 0; i < length; ++i)
			{
				_tx_packet.PutData(data[i]);
			}
			this->_Send();
			
			return;
		}
	
	private:
		void _Send()
		{
			_tx_packet.Transport(this->_transport);
			_tx_packet.Direction(0x01);
			
			if(this->_urgent_data == true) _tx_packet.Urgent(1);
			this->_urgent_data = false;
			
			_tx_packet.Prepare();
			
			byte txbyte;
			while( _tx_packet.GetPacketByte(txbyte) == true )
			{
				this->_driver->SendByte(txbyte);
			}
			
			_tx_packet.Init();
			
			return;
		}
		
		packet_t _rx_packet;
		packet_t _tx_packet;
		callback_event_t _callback_event;
		callback_error_t _callback_error;
		
		L3Driver *_driver;
		
		uint8_t _transport;
		bool _urgent_data = false;
};
