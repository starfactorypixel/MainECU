/*
	Пример программы эмулятора устройств.
*/

#include "emulator.h"

Emulator em;

//								uint32_t id, T min, T max, uint16_t interval, T step, T value, algorithm_t algorithm
VirtualDevice<float> dev_voltage(12345,		62.0,		82.0,		2500,		0.25,		74.32,		VirtualDevice<float>::ALG_RANDOM);
VirtualDevice<uint8_t> dev_speed(17091,		0,			101,		750,		1,			2,			VirtualDevice<uint8_t>::ALG_MINFADEMAX);
VirtualDevice<int32_t> dev_current(3472,	-150000,	150000,		1000,		250,		-1124,		VirtualDevice<int32_t>::ALG_RANDOM);

void setup()
{
	Serial.begin(115200);
	Serial.println("Start");
	
	em.RegDevice(dev_voltage);
	em.RegDevice(dev_speed);
	em.RegDevice(dev_current);
	

	
	return;
}

uint32_t current_time = 0;
uint32_t lastSec = 0;

void loop()
{
	current_time = millis();
	
	em.Processing(current_time);
	
	if(current_time - lastSec > 1000)
	{
		lastSec = current_time;
		
		GetID(12345);
		GetID(17091);
		GetID(3472);
		GetID(6666);
		
		Serial.println();
	}
	
	
	
	
	
	
	
	return;
}

void GetID(uint32_t id)
{
	//uint32_t id = 3472;
	uint8_t bytes[8];
	uint8_t length;
	if( em.Request(id, bytes, length) == true )
	{
		Serial.print(">> id = ");
		Serial.print(id);
		Serial.print(", bytes = '");
		
		for(uint8_t i = 0; i < length; ++i)
		{
			if(bytes[i] < 0x10) Serial.print("0");
			Serial.print(bytes[i], HEX);
			Serial.print(" ");
		}
		
		//Serial.write(bytes, length);
		
		Serial.print("', length = ");
		Serial.print(length);
		Serial.println();
	}
	else
	{
		Serial.print(">> ID = ");
		Serial.print(id);
		Serial.println(" not found!");
	}
}