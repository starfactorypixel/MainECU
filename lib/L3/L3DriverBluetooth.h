/*
	Драйвер работы с нативным Bluetooth ESP32.
*/

#pragma once

#include <BluetoothSerial.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"

class L3DriverBluetooth final : public L3Driver
{
	public:
		void Init() override
		{
			char btDevName[20];
			SerialBT.begin(" "); // инициализируем BT с пустой строкой вместо имени
			const uint8_t* mac = esp_bt_dev_get_address(); //вытаскиваем mac адрес BT
			sprintf(btDevName, "StarPixel_%02X%02X%02X", mac[3], mac[4], mac[5]); //последние 3 байта адреса пишем в имя BT
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
