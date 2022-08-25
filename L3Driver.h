/*

*/

#pragma once

#include <SoftwareSerial.h>

class L3Driver
{
	public:
		virtual void Init(){ }
		virtual uint8_t ReadAvailable(){ }
		virtual byte ReadByte(){ }
		virtual void SendByte(byte data){ }
};

/*
	Драйвер работы с нативным uart.
	Использует пины: Согласно Serial.
	Скорость: 115200.
*/
class L3DriverRAW : public L3Driver
{
	public:
		void Init() override
		{
			Serial.begin(115200);
			
			return;
		}
		
		uint8_t ReadAvailable() override
		{
			return Serial.available();
		}
		
		byte ReadByte() override
		{
			return Serial.read();
		}
		
		void SendByte(byte data) override
		{
			Serial.write(data);
			
			return;
		}
};

class L3DriverUART : public L3Driver
{
	
};

class L3DriverBluetooth : public L3Driver
{
	
};


/*
	Драйвер работы с виртуальным uart.
	Использует пины: RX:2, RX:3.
	Скорость: 19200.
*/
class L3DriverSoftSerial : public L3Driver
{
	public:
		L3DriverSoftSerial() : mySerial(2, 3)
		{
			return;
		}
		
		void Init() override
		{
			mySerial.begin(19200);
			
			return;
		}
		
		uint8_t ReadAvailable() override
		{
			return mySerial.available();
		}
		
		byte ReadByte() override
		{
			return mySerial.read();
		}
		
		void SendByte(byte data) override
		{
			mySerial.write(data);
			
			return;
		}
	
	private:
		SoftwareSerial mySerial;
};
