/*
	Драйвер работы с нативным Bluetooth ESP32.
*/

#pragma once

#include <BluetoothSerial.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"
//#include <esp_gap_ble_api.h>
//#include <esp_gap_bt_api.h>
//#include <esp_bt.h>

class L3DriverBluetooth final : public L3Driver
{
	public:
		
		L3DriverBluetooth()
		{
			_type = L3_DEVTYPE_BLUETOOTH;
			_rx_packet.SetTimeout(50);
			
			return;
		}
		
		void Init() override
		{
			char btDevName[20];
			

			//char pin[9]; pin[8] = '\0';
			//GetPinCode(pin);
			//SerialBT.enableSSP();

			
			SerialBT.begin(" "); // инициализируем BT с пустой строкой вместо имени
			const uint8_t* mac = esp_bt_dev_get_address(); //вытаскиваем mac адрес BT
			sprintf(btDevName, "StarPixel_%02X%02X%02X", mac[3], mac[4], mac[5]); //последние 3 байта адреса пишем в имя BT
			esp_bt_dev_set_device_name(btDevName); //переименовываем BT

			//esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
        	//esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
        	//esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
			
			//SerialBT.setPin("555555");



	//esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);

	//esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
	//esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, 1);

	//esp_bt_pin_code_t pin = "555555";
	//esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 6, pin);






			
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
		
		void SendBytes(const uint8_t *buffer, uint8_t length) override
		{
			SerialBT.write(buffer, length);
			
			return;
		}
		
	private:
		BluetoothSerial SerialBT;
};
