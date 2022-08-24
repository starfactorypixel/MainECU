/*
	Пример программы получения запроса и отправки ответа.
	В частном случае это MainECU.
*/

#include <SoftwareSerial.h>
#include "packet.h"
#include "L3Driver.h"
#include "L3Wrapper.h"


//L3DriverRAW driver_raw;
L3DriverSoftSerial driver_ss;
L3Wrapper Protocol(0, driver_ss);

void setup()
{
	Serial.begin(115200);
	
	Protocol.RegCallback(OnRX);
	Protocol.Init();
	
	return;
}

void loop()
{
	Protocol.IncomingByte();
	
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
