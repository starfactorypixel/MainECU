#include "packet.h"



/*
struct StarPixelHighPacket_t
{
	uint8_t start_byte;
	
	uint8_t version:3;
	uint8_t transport:2;
	byte reserve_1:3;
	
	uint8_t direction:1;
	uint8_t urgent:1;
	byte reserve_2:1;
	uint8_t type:5;
	
	uint16_t param;
	
	uint8_t length;
	
	byte data[64];
	
	uint16_t crc;
	
	uint8_t stop_byte;
} mypacket;
*/







class L3Wrapper
{
	public:
		using packet_t = StarPixelHighPacket<64>;
		using callback_t = bool (*)(packet_t &request, packet_t &response);
		//using callback2_t = void (*)(uint8_t type, uint16_t param, uint8_t length, byte *data);
		
		L3Wrapper()
		{
			_tx_packet.Transport(3);
			_tx_packet.Direction(1);
			
			return;
		}
		
		void RegCallback(callback_t callback)
		{
			this->_callback = callback;
		}
		
		// В будущем будет заменён на прерывание приёма байта.
		void IncomingByte()
		{
			byte incomingByte = Serial.read();
			
			if( _rx_packet.PutPacketByte(incomingByte) == true )
			{
				if( _rx_packet.IsReceived() == true )
				{
					if( this->_callback(_rx_packet, _tx_packet) == true )
					{
						// Отправка ответа.
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
				
				while(Serial.available() > 0){ Serial.read(); }
			}
			
			return;
		}
	
	private:
		packet_t _rx_packet;
		packet_t _tx_packet;
		callback_t _callback;
};


L3Wrapper Protocol;




void setup()
{
	Serial.begin(115200);
	
	Protocol.RegCallback(OnRX);
	
	return;
}

void loop()
{
	if( Serial.available() > 0 )
	{
		Protocol.IncomingByte();
	}
	
	return;
}

bool OnRX(L3Wrapper::packet_t &request, L3Wrapper::packet_t &response)
{
	
	byte data_arr[ request.GetDataLength() ];
	for(uint8_t i = 0; i < request.GetDataLength(); ++i)
	{
		byte data;
		request.GetData(data);
		data_arr[i] = data;
	}
	
	
	Serial.print("Type: ");
	Serial.println( request.Type() );
	
	Serial.print("Param: ");
	Serial.println( request.Param() );
	
	if(request.Param() == 65000)
	{
		uint32_t time = (((uint32_t)data_arr[0] << 24) | ((uint32_t)data_arr[1] << 16) | ((uint32_t)data_arr[2] << 8) | ((uint32_t)data_arr[3]));
		
		Serial.print("Time: ");
		Serial.println( time );
	}
	
	Serial.print("GetData: ");
	for(uint8_t i = 0; i < sizeof(data_arr); ++i)
	{
		if(data_arr[i] < 0x10) Serial.print("0");
		Serial.print(data_arr[i], HEX);
		Serial.print(" ");
	}
	
	Serial.println();
	Serial.println();
}




















/* ------------- RX -------------- */
/*
StarPixelHighPacket<64> RXObj;

void setup()
{
	
	
	
	
	
	
	
	
	
	
	
	
	
	Serial.begin(115200);
	
	RXObj.Transport(3);
	RXObj.Direction(1);
	
	return;
}

void loop()
{
	
	while(Serial.available() > 0)
	{
		byte incomingByte = Serial.read();
		
		if( RXObj.PutPacketByte(incomingByte) == true )
		{
			int8_t error;
			if( RXObj.IsReceived(error) == true )
			{
				Serial.print("Type: ");
				Serial.println( RXObj.Type() );
				
				Serial.print("Param: ");
				Serial.println( RXObj.Param() );
				
				if(RXObj.Param() == 65000)
				{
					
					uint32_t time = 0;
					
					for(int8_t i = 3; i >= 0; --i)
					{
						byte data;
						RXObj.GetData(data);
						time |= (data << i*8);
					}
					
					Serial.print("Time: ");
					Serial.println( time );
				
				}
				else
				{
					Serial.print("GetData: ");
					byte data;
					while( RXObj.GetData(data) == true )
					{
						Serial.print(data, HEX);
						Serial.print(" ");
					}
					Serial.println();
				}
				
				RXObj.Init();
				
				Serial.println();
			}
			
			if(error < 0)
			{
				Serial.print("Error: ");
				Serial.println( error );
				
				RXObj.Init();
			}
		}
		else
		{
			Serial.println("ERROR_OVERFLOW");
			
			RXObj.Init();
			while(Serial.available() > 0){ Serial.read(); }
		}
	}
	
	return;
}
*/
/* ------------- RX -------------- */









/* ------------- TX -------------- */
/*
StarPixelHighPacket<64> TXObj;

void setup()
{
	Serial.begin(115200);
	
	TXObj.Transport(3);
	TXObj.Type(5);
	TXObj.Param(21464);
	
	TXObj.Data1(0x12);
	TXObj.Data1(0x37);
	TXObj.Data1(0x4A);
	TXObj.Data1(0x39);
	TXObj.Data1(0xEA);
	TXObj.Data1(0x69);
	
	TXObj.Prepare();
	
	byte data;
	while( TXObj.GetPacketByte(data) == true )
	{
		Serial.write(data);
	}
	
	return;
}

void loop()
{
	TXObj.Init();
	
	TXObj.Transport(3);
	TXObj.Type(30);
	TXObj.Param(65000);
	
	uint32_t time = millis();
	for(int8_t i = 3; i >= 0; --i)
	{
		TXObj.Data1( ((time >> i*8) & 0xFF) );
	}
	
	TXObj.Prepare();
	
	byte data;
	while( TXObj.GetPacketByte(data) == true )
	{
		Serial.write(data);
	}
	
	delay(750);
	
	return;
}
*/
/* ------------- TX -------------- */