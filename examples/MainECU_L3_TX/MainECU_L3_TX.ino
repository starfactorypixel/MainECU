/*
	Пример программы отправки запроса и получения ответа.
	В частном случае это планшет, телефон, приборная панель.
*/

#include <SoftwareSerial.h>
#include "packet.h"

SoftwareSerial txserial(2, 3);

StarPixelHighPacket<64> TXObj;

void setup()
{
	txserial.begin(19200);
	
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
		txserial.write(data);
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
		txserial.write(data);
	}
	
	delay(750);
	
	return;
}
