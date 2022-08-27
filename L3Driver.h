/*

*/

#pragma once

// #include <SoftwareSerial.h>
#include <BluetoothSerial.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"

class L3Driver
{
	public:
		virtual void Init(){ }
		virtual uint8_t ReadAvailable(){ }
		virtual uint8_t ReadByte(){ }
		virtual void SendByte(uint8_t data){ }
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

class L3DriverUART : public L3Driver
{
	
};

/*
	Драйвер работы с bluetooth.
*/
class L3DriverBluetooth : public L3Driver
{
	public:
		void Init() override
		{
			char btDevName[20];
			SerialBT.begin(" "); // инициализируем BT с пустой строкой вместо имени
			const uint8_t* mac = esp_bt_dev_get_address(); //вытаскиваем mac адрес BT
			sprintf(btDevName, "StarPixel %02X:%02X:%02X", mac[3], mac[4], mac[5]); //последние 3 байта адреса пишем в имя BT
			esp_bt_dev_set_device_name(btDevName); //переименовываем BT
			return;
		}
		
		uint8_t ReadAvailable() override
		{
			return SerialBT.available();
		}
		
		uint8_t ReadByte() override
		{
			return SerialBT.read();
		}
		
		void SendByte(uint8_t data) override
		{
			SerialBT.write(data);
			
			return;
		}
	
	private:
		BluetoothSerial SerialBT;		
};


/*
	Драйвер работы с виртуальным uart.
	Использует пины: RX:2, RX:3.
	Скорость: 19200.
*/
// class L3DriverSoftSerial : public L3Driver
// {
// 	public:
// 		L3DriverSoftSerial() : mySerial(2, 3)
// 		{
// 			return;
// 		}
		
// 		void Init() override
// 		{
// 			mySerial.begin(19200);
			
// 			return;
// 		}
		
// 		uint8_t ReadAvailable() override
// 		{
// 			return mySerial.available();
// 		}
		
// 		uint8_t ReadByte() override
// 		{
// 			return mySerial.read();
// 		}
		
// 		void SendByte(uint8_t data) override
// 		{
// 			mySerial.write(data);
			
// 			return;
// 		}
	
// 	private:
// 		SoftwareSerial mySerial;
// };
