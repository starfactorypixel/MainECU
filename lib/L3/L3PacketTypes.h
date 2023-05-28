#pragma once

#include <inttypes.h>

namespace L3PacketTypes
{
	// Пакет авторизации, запрос Android => Main.
	struct auth_req_t
	{
		uint8_t funcID;				// 0x02
		uint8_t method;				// Способ авторизации. sha1(sn[8]+rand[16]) = 0x01.
		uint8_t rand_str[16];		// Случайная строка, 16 байт.
		uint8_t hash_str[20];		// Результат sha1( serial + rand )
	};
	
	// Пакет авторизации, ответ Main => Android.
	struct auth_resp_t
	{
		uint8_t funcID;				// 0x03
		uint8_t code;				// Код ответа, 1 - успешно, 0 - ошибка.
		uint32_t time;				// Uptime Main, мс.
	};
}
