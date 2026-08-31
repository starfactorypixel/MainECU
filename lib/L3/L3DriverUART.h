/*
	Драйвер работы с нативным UART.
*/

#pragma once

#include <HardwareSerial.h>

class L3DriverUART final : public L3Driver
{
	public:
		
		L3DriverUART() : SerialHW(0)
		{
			_type = L3_DEVTYPE_COMPUTER;
			_rx_packet_hot.SetTimeout(10);
			
			return;
		}
		
		void Init() override
		{
			SerialHW.begin(921600, SERIAL_8N1, GPIO_NUM_43, GPIO_NUM_44);
			
			return;
		}
		
		uint8_t ReadAvailable() override
		{
			return SerialHW.available();
		}
		
		uint8_t ReadByte() override
		{
			return SerialHW.read();
		}
		
		void SendByte(uint8_t data) override
		{
			SerialHW.write(data);
			
			return;
		}
		
		void SendBytes(const uint8_t *buffer, uint8_t length) override
		{
			SerialHW.write(buffer, length);
			SerialHW.flush();
			
			return;
		}

	private:
		
		HardwareSerial SerialHW;
};
