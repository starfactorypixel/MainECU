#pragma once
#include <inttypes.h>

class L3DriverInterface
{
	public:
		
		// Битовая маска типов L3 устройств.
		enum dev_type_t : uint8_t
		{
			DEVTYPE_NONE = 0x00,
			DEVTYPE_DASHBOARD = (1U << 0),		// Приборная панель по UART
			DEVTYPE_BLUETOOTH = (1U << 1),		// Устройство по Bluetooth
			DEVTYPE_SUBGHZ =    (1U << 6),		// Устройство по радиоканалу
			DEVTYPE_SERVER =    (1U << 7),		// Облачный сервер через LTE модем
			DEVTYPE_ALL = 0xFF					// Все устройства
		};
		
		// Состояние L3 устройства.
		enum dev_state_t : uint8_t
		{
			DEVSTATE_NONE = 0x00,
			DEVSTATE_IDLE,						// Устройство не подключено
			DEVSTATE_ACTIVE,					// Устройство подключено
			DEVSTATE_TIMEOUT,					// Устройство перестало отвечать
			DEVSTATE_AUTHERR,					// Устройство провалило проверку авторизации или подписи
		};
		
		
		virtual void SendData(const uint8_t *data, uint16_t length) = 0;
		
};
