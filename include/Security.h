#pragma once

#include <inttypes.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_tls_crypto.h>
#include <string.h>

#include <Config.h>
#include <L3PacketTypes.h>

namespace Security
{
	bool GetRandomBytes(uint8_t *bytes, uint8_t length)
	{
		if(bytes == nullptr || length == 0 || length > 32) return false;
		
		esp_fill_random(bytes, length);
		
		return true;
	}
	
	void CreateSerial()
	{
		if( GetRandomBytes(Config::obj.security.serial, sizeof(Config::obj.security.serial)) == true )
		{
			Config::obj.security.sn_init = true;
			Config::Save();
		}
		
		return;
	}
	
	bool CheckAuth(L3PacketTypes::auth_req_t *packet)
	{
		bool result = false;
		
		// Авторизация по SHA1 со случайным ключём 16 байт.
		if(packet->method = 0x01)
		{
			const uint8_t random_length = 16;
			const uint8_t local_sn_length = sizeof(Config::obj.security.serial);
			
			// Массив для склееной строки: Серийный номер и Случайный ключ.
			uint8_t local_data[ local_sn_length + random_length ];
			
			// Массив для хранения вычесленного SHA1.
			uint8_t local_hash[20];
			
			// Собираем строку SN + Rand.
			memcpy(local_data, Config::obj.security.serial, local_sn_length);
			memcpy(local_data+local_sn_length, packet->rand_str, random_length);
			
			// Считаем SHA1.
			esp_crypto_sha1(local_data, sizeof(local_data), local_hash);
			
			// Проверяем результат с полученным хешем.
			if( memcmp(packet->hash_str, local_hash, sizeof(local_hash)) == 0 )
			{
				result = true;
			}
		}
		
		return result;
	}
	
	
	inline void Setup()
	{
		if(Config::obj.security.sn_init == false)
		{
			CreateSerial();
		}
/*
		L3PacketTypes::auth_req_t q;
		q.method = 0x01;
		q.rand_str[0] = 0x01;
		q.rand_str[1] = 0x02;
		q.rand_str[2] = 0x03;
		q.rand_str[3] = 0x04;
		q.rand_str[4] = 0x05;
		q.rand_str[5] = 0x06;
		q.rand_str[6] = 0x07;
		q.rand_str[7] = 0x08;
		q.rand_str[8] = 0xA9;
		q.rand_str[9] = 0x0A;
		q.rand_str[10] = 0x0B;
		q.rand_str[11] = 0x0C;
		q.rand_str[12] = 0x0D;
		q.rand_str[13] = 0x0E;
		q.rand_str[14] = 0x0F;
		q.rand_str[15] = 0x10;
		//q.hash_str = {0x18,0xC7,0xF3,0x2A,0xC5,0x4B,0xE7,0xC4,0x2D,0x59,0xBD,0x81,0x36,0x58,0x75,0x38,0x36,0x12,0xBC,0x37};

		uint8_t hash[] = {0x18,0xC7,0xF3,0x2A,0xC5,0x4B,0xE7,0xC4,0x2D,0x59,0xBD,0x81,0x36,0x58,0x75,0x38,0x36,0x12,0xBC,0x37};
		memcpy(q.hash_str, hash, 20);

		if( CheckAuth(q) == true )
		{
			Serial.printf("HASH VALID!");
		}
		else
		{
			Serial.printf("HASH NOT VALID!");
		}
*/		
		return;
	}

	inline void Loop(uint32_t &time)
	{
		time = millis();

		return;
	}
}
