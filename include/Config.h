#pragma once
#include <inttypes.h>
#include <esp_system.h>
#include <EEPROM.h>
#include "ConfigData.h"
#include <CUtils_Crypto.h>

namespace Config
{
	static constexpr uint16_t eeprom_offset = 0;

	// Блок данных, временно, потом обернуть как в https://github.com/starfactorypixel/SlaveECU_SteeringRack/blob/board_v2.1/include/Config.h

	struct config_t
	{
		uint8_t version = 3;
		config_body_t data = {};
	} config_obj;
	
	config_body_t &Obj()
	{
		return config_obj.data;
	}

	void FirstSetup();
	
	void Save()
	{
		DEBUG_LOG_TOPIC("CFG", "Save()\n");
		
		EEPROM.put(eeprom_offset, config_obj);
		EEPROM.commit();
		
		return;
	}
	
	void Load()
	{
		if(EEPROM.readByte(eeprom_offset) != config_obj.version)
		{
			FirstSetup();
			Save();
		}
		EEPROM.get(eeprom_offset, config_obj);
		
		return;
	}
	
	void Reset()
	{
		EEPROM.writeByte(eeprom_offset, 0xFF);
		EEPROM.commit();
		esp_restart();
		
		return;
	}

	void FirstSetup()
	{
		// Генерируем серийный номер
		auto &sn = Obj().security.sn;
		GetSerialNumber64(sn);
		
		// Генерируем имя по умолчанию
		auto &name = Obj().security.name;
		snprintf(name, sizeof(name), "PixelMini %02X%02X%02X", sn[3], sn[4], sn[5]);

		// Генерируем приватные ключи
		auto &key = Obj().security.dev_01.key;
		esp_fill_random(key, sizeof(key));
		
		Obj().security.init = true;
		
		Save();

		// Показываем SN и KEY и зависаем.
		Logger.PrintTopic("Crypto").Print("sn: ").Print(sn, sizeof(sn), LOG_OUT_TYPE_HEX).PrintNewLine();
		Logger.PrintTopic("Crypto").Print("key: ").Print(key, sizeof(key), LOG_OUT_TYPE_HEX).PrintNewLine();
		while(true){}
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
