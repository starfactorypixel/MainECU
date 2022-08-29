/*
	Драйвер работы с виртуальным UART.
	Использует пины: RX:2, RX:3.
	Скорость: 19200.
*/

#pragma once

#include <SoftwareSerial.h>

class L3DriverSoftSerial final : public L3Driver
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
		
		uint8_t ReadByte() override
		{
			return mySerial.read();
		}
		
		void SendByte(uint8_t data) override
		{
			mySerial.write(data);
			
			return;
		}
		
	private:
		SoftwareSerial mySerial;
};
