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
		L3DriverSoftSerial() : SerialSoft(2, 3)
		{
			return;
		}
		
		void Init() override
		{
			SerialSoft.begin(19200);
			
			return;
		}
		
		uint8_t ReadAvailable() override
		{
			return SerialSoft.available();
		}
		
		uint8_t ReadByte() override
		{
			return SerialSoft.read();
		}
		
		void SendByte(uint8_t data) override
		{
			SerialSoft.write(data);
			
			return;
		}
		
		void SendBytes(const uint8_t *buffer, uint8_t length) override
		{
			SerialSoft.write(buffer, length);
			
			return;
		}
		
	private:
		SoftwareSerial SerialSoft;
};
