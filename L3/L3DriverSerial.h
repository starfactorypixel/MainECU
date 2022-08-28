/*
	Драйвер работы с объектом Serial.
	Использует пины: Согласно Serial.
	Скорость: 115200.
*/

#pragma once

class L3DriverSerial final : public L3Driver
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
		
		uint8_t ReadByte() override
		{
			return Serial.read();
		}
		
		void SendByte(uint8_t data) override
		{
			Serial.write(data);
			
			return;
		}
};
