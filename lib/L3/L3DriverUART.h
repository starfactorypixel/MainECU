/*
	Драйвер работы с нативным UART.
*/

#pragma once

class L3DriverUART final : public L3Driver
{
	public:
		void Init() override
		{
			return;
		}
		
		uint8_t ReadAvailable() override
		{
			return 0;
		}
		
		uint8_t ReadByte() override
		{
			return 0x00;
		}
		
		void SendByte(uint8_t data) override
		{
			return;
		}
};
