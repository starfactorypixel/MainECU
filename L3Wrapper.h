/*

*/

#pragma once

class L3Wrapper
{
	public:
		using packet_t = StarPixelHighPacket<64>;
		using callback_t = bool (*)(packet_t &request, packet_t &response);
		
		L3Wrapper(uint8_t transport, L3Driver &driver) : _driver(&driver)
		{
			this->_transport = transport;
			
			return;
		}
		
		void Init()
		{
			this->_driver->Init();
		}
		
		void RegCallback(callback_t callback)
		{
			this->_callback = callback;
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
				
				if( _rx_packet.PutPacketByte(incomingByte) == true )
				{
					if( _rx_packet.IsReceived() == true )
					{
						if( this->_callback(_rx_packet, _tx_packet) == true )
						{
							// Установка транспорта ( перенести в packet.h ? )
							_tx_packet.Transport(this->_transport);
							
							// Флаг ответа.
							_tx_packet.Direction(1);
							
							// Флаг необходимости передать срочное сообщение.
							if(this->_urgent_data == true) _tx_packet.Urgent(1);
							this->_urgent_data = false;
							
							// Отправка ответа.
							byte data;
							while( _tx_packet.GetPacketByte(data) == true )
							{
								this->_driver->SendByte(data);
							}
							
							// Очистка пакета.
							_tx_packet.Init();
						}
						_rx_packet.Init();
					}
					
					if(_rx_packet.GetError() < 0)
					{
						Serial.print("Error: ");
						Serial.println( _rx_packet.GetError() );
						
						_rx_packet.Init();
					}
				}
				else
				{
					Serial.println("ERROR_OVERFLOW");
					
					_rx_packet.Init();
					
					// Или метод FlushBuffer() ?
					while(this->_driver->ReadAvailable() > 0){ this->_driver->ReadByte(); }
				}
			}
			
			return;
		}
	
	private:
		packet_t _rx_packet;
		packet_t _tx_packet;
		callback_t _callback;
		
		L3Driver *_driver;
		
		uint8_t _transport;
		bool _urgent_data = false;
};
