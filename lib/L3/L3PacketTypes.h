#pragma once

#include <inttypes.h>

namespace L3PacketTypes
{
	
	// Пакет Ping, Android => Main и Main => Android.
	struct ping_t
	{
		uint8_t funcID;				// 0x01
		uint32_t time;				// Uptime, мс.
	};
	
	
	// Пакет авторизации, запрос Android => Main.
	struct auth_req_t
	{
		uint8_t funcID;				// 0x02
		uint8_t method;				// Способ авторизации. SHA1( sn[8] + rand[16] ) = 0x01
		uint8_t rand_str[16];		// Случайная строка, 16 байт
		uint8_t hash_str[20];		// Результат SHA1( sn[8] + rand[16] )
	};
	
	// Пакет авторизации, ответ Main => Android.
	struct auth_resp_t
	{
		uint8_t funcID;				// 0x03
		uint8_t code;				// Код ответа, 1 - успешно, 0 - ошибка
		uint32_t time;				// Uptime Main, мс.
	};
	
	
	// 
	struct can_frame_t
	{
		uint8_t funcID;				// CAN ID функции
		uint8_t data[7];			// CAN данные
		uint8_t data_len;			// CAN длина данных
	};
	
	
	// Пакет подписки на параметр, запрос Android => Main.
	struct subscribe_req_t
	{
		uint8_t funcID;				// 0x04
		uint16_t devID;				// ID устройства на шине CAN, или виртуальное устройство, если > 2047
		uint8_t param;				// Доп. параметр подписки, 1 - Подписаться, 0 - Отписаться
	};
	
	// Пакет подписки на параметр, ответ Main => Android.
	struct subscribe_resp_t
	{
		uint8_t funcID;				// 0x05
		uint16_t devID;				// ID устройства на шине CAN, или виртуальное устройство, если > 2047
		uint8_t code;				// Код ответа, 1 - успешно, 0 - ошибка
		can_frame_t can_frame;		// Последний CAN пакет от устройства
	};
	
	
	// Пакет события, Android => Main и Main => Android.
	struct event_t
	{
		uint8_t funcID;				// 0x15
		uint16_t devID;				// ID устройства на шине CAN, или виртуальное устройство, если > 2047
		uint8_t param;				// Доп. параметр события
		can_frame_t can_frame;		// CAN пакет
	};


	// Пакет ошибки протокола L3, ответ Main => Android.
	struct error_t
	{
		uint8_t funcID;				// 0xE0
		uint8_t sourceID;			// funcID источника ошибки
		uint8_t code;				// Код ошибки
	};






}
