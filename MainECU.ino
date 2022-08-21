#include "packet.h"




/* ------------- RX -------------- */

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