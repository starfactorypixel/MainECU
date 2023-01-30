/*
	Драйвер работы с нативным UART.
*/

#pragma once

#include <HardwareSerial.h>

class L3DriverUART final : public L3Driver
{
	public:
		
		L3DriverUART() : SerialUART(2)
		{
			_type = L3_DEVTYPE_COMPUTER;
			_rx_packet.SetTimeout(10);
			
			return;
		}
		
		void Init() override
		{
			SerialUART.begin(115200, SERIAL_8N1, GPIO_NUM_25, GPIO_NUM_26);
			
			return;
		}
		
		uint8_t ReadAvailable() override
		{
			return SerialUART.available();
		}
		
		uint8_t ReadByte() override
		{
			return SerialUART.read();
		}
		
		void SendByte(uint8_t data) override
		{
			SerialUART.write(data);
			
			return;
		}

	private:
		
		HardwareSerial SerialUART;
};
