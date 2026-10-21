#pragma once

#include <inttypes.h>
#include <esp_system.h>
#include <EEPROM.h>
#include "ConfigData.h"

namespace Config
{
	static constexpr uint16_t eeprom_offset = 0;






	// Блок данных, временно, потом обернуть как в https://github.com/starfactorypixel/SlaveECU_SteeringRack/blob/board_v2.1/include/Config.h
	config_body_t body;

	config_body_t &Obj()
	{
		return body;
	}


	
	struct config_t
	{
		uint8_t version = 1;
		
		struct 
		{
			bool sn_init = false;
			uint8_t serial[8];
		} security;
	} obj;
	
	
	void Save()
	{
		EEPROM.put(eeprom_offset, obj);
		EEPROM.commit();
		
		return;
	}
	
	void Load()
	{
		if( EEPROM.readByte(eeprom_offset) != obj.version )
		{
			Save();
		}
		EEPROM.get(eeprom_offset, obj);
		
		return;
	}
	
	void Reset()
	{
		EEPROM.writeByte(eeprom_offset, 0xFF);
		EEPROM.commit();
		esp_restart();
		
		return;
	}
	
	
	inline void Setup()
	{
		EEPROM.begin(256);
		Load();


		// Временное наполнение данных
		const uint8_t sn[32] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
		memcpy(body.security.sn, sn, sizeof(body.security.sn));
		const uint8_t key[32] = {0x8F, 0x3D, 0x7A, 0x1B, 0xC4, 0x5E, 0x92, 0x0D, 0x6F, 0x4A, 0x2B, 0xEC, 0xD1, 0x83, 0x75, 0x9A, 0x14, 0xFE, 0x60, 0xBB, 0x29, 0xC7, 0x53, 0xA8, 0x4E, 0xD0, 0x31, 0x9F, 0x62, 0xB5, 0x7C, 0xE6};
		memcpy(body.security.dev_01.key, key, sizeof(body.security.dev_01.key));
		const char name[] = "DragonPixel1234";
		memcpy(body.security.name, name, sizeof(name));
		
		return;
	}

	inline void Loop(uint32_t &time)
	{
		
		
		time = millis();

		return;
	}
}
