#pragma once

#include <inttypes.h>
#include <esp_system.h>
#include <EEPROM.h>

namespace Config
{
	static constexpr uint16_t eeprom_offset = 0;
	
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
		
		return;
	}

	inline void Loop(uint32_t &time)
	{
		
		
		time = millis();

		return;
	}
}
