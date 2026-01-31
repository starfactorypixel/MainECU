#pragma once
#include <inttypes.h>
#include <string.h>
#include "esp_crc.h"
#include <DrakeScriptCore.hpp>
#include "ScriptLogicCustom.h"

namespace ScriptLogic
{
	static constexpr uint16_t SCRIPTS_COUNT = 2048;
	static constexpr uint32_t PSRAM_MALLOC_SIZE = SCRIPTS_COUNT * 512;
	
	DrakeScriptMapping ScriptMapObj;
	DrakeScriptCore ScriptObj;



//uint8_t *script_data = (uint8_t *)heap_caps_malloc(2048*512, MALLOC_CAP_SPIRAM);
uint8_t *script_data = nullptr;

/*

	Флешка W25Q128JV. 16 МБ.
	Каждый скрипт занимает ровно один сектор = 4 КБ. Всего под скрипты нужно 2048 секторов = 8 МБ.
	Скрипты расположены во второй половине флешки, т.е. начиная с адреса 8388608 или 2048 сектора.
	Смещение байт = 8388608 + (4096 * ID скрипта);
	Смещение сектор = (8388608 / 4096) + ID скрипта;

	Формат скрипта:
		Заголовок, 16 байт: [Длина, байт] [CRC32]
		Данные:
	Общая длина скрипта долна быть крастка 4 байтам. Если нет, то дополнена нулевыми байтами.


*/

/*
	Тут мы грузим скрипты из SPI flash в массив PSRAM.
	После загрузки каждого дёргаем `ScriptMapObj.AddScriptMap(0x1234, 0, 1024);`
	ID скрипта, начало смещения в массиве PSRAM, длина скрипта в массиве PSRAM.
*/

void ScriptUsedTest()
{


	uint8_t data[] = {0x01, 0xFF};
	ScriptObj.Trigger(0x123, data, sizeof(data));
}





	uint32_t calc_crc_inplace(uint8_t *buff);


	static constexpr uint32_t NOR_OFFSET = 8 * 1024 * 1024;
	static constexpr uint16_t NOR_SECTOR_SIZE = 4 * 1024;

	struct __attribute__((packed)) nor_script_header_h
	{
		uint8_t flag1;
		uint8_t flag2;
		uint16_t length;
		uint32_t crc32;
		uint8_t _padding[8];
	};

	void LoadFromSPIFlash()
	{
		uint8_t *nor_buffer = (uint8_t *)heap_caps_malloc(NOR_SECTOR_SIZE, MALLOC_CAP_INTERNAL);
		uint32_t nor_read_offset;
		nor_script_header_h *nor_script_header = (nor_script_header_h *) nor_buffer;

		uint32_t psram_write_offset = 0;

		for(uint16_t script_id = 0; script_id < SCRIPTS_COUNT; ++script_id)
		{
			memset(nor_buffer, 0x00, NOR_SECTOR_SIZE);
			nor_read_offset = NOR_OFFSET + (NOR_SECTOR_SIZE * script_id);
			
			//SPI:ReadBytes(nor_read_offset, NOR_SECTOR_SIZE, nor_buffer);
			
			if(nor_script_header->flag1 == 0x55)
			{
				if(nor_script_header->length > (NOR_SECTOR_SIZE - sizeof(nor_script_header_h)))
				{
					DEBUG_LOG_TOPIC("PSRAM", "Invalid script length! id:%d, len:%u\n", script_id, nor_script_header->length);
					continue;
				}
				
				if(calc_crc_inplace(nor_buffer) != nor_script_header->crc32)
				{
					DEBUG_LOG_TOPIC("PSRAM", "Script CRC error! id:%d\n", script_id);
					continue;
				}
				
				if(psram_write_offset + nor_script_header->length <= PSRAM_MALLOC_SIZE)
				{
					DEBUG_LOG_TOPIC("PSRAM", "Mo more PSRAM memory! id:%d\n", script_id);
					break;
				}
				
				memcpy(&script_data[psram_write_offset], &nor_buffer[sizeof(nor_script_header_h)], nor_script_header->length);
				
				ScriptMapObj.AddScriptMap(script_id, psram_write_offset, nor_script_header->length);
				
				psram_write_offset += nor_script_header->length;

			}
		}
		
		heap_caps_free(nor_buffer);
		
		return;
	}



	// Массив data должен быть кратен 256 байтам и заполнен 0xFF по умолчанию.
	void SaveToSPIFlash(uint16_t script_id, uint8_t *data, uint16_t length)
	{
		if(script_id >= SCRIPTS_COUNT) return;
		if(length > NOR_SECTOR_SIZE) return;
		if(length % 256) return;
		
		uint32_t nor_offset = NOR_OFFSET + (NOR_SECTOR_SIZE * script_id);
		uint32_t sector = nor_offset / NOR_SECTOR_SIZE;
		uint32_t page = nor_offset / 256;
		uint8_t page_count = (length + 255) / 256;
		
		//SPI::EraseSector(sector);
		//SPI::WaitReady();

		for(uint8_t i = 0; i < page_count; ++i)
		{
			//SPI::WritePage(page + i, &data[i * 256]);
			//SPI::WaitReady();
		}

		return;
	}




	
	uint32_t calc_crc_inplace(uint8_t *buff)
	{
		nor_script_header_h *hdr = (nor_script_header_h *) buff;
		uint32_t saved_crc = hdr->crc32;
		hdr->crc32 = 0x00000000;
		
		uint32_t crc = esp_crc32_le(0xFFFFFFFF, buff, sizeof(nor_script_header_h) + hdr->length) ^ 0xFFFFFFFF;
		
		hdr->crc32 = saved_crc;
		
		return crc;
	}
	
	
	inline void Setup()
	{
		script_data = (uint8_t *)heap_caps_malloc(PSRAM_MALLOC_SIZE, MALLOC_CAP_SPIRAM);
		if(!script_data)
		{
			DEBUG_LOG_TOPIC("PSRAM", "PSRAM alloc failed\n");
			return;
		}
		
		LoadFromSPIFlash();
		ScriptMapObj.SetScriptsArray(script_data, PSRAM_MALLOC_SIZE);
		
		ScriptObj.AddScriptMap(ScriptMapObj);
		ScriptObj.RegCustomOpcode((opcode_idx_t)0xA0, TestOpcode);
		ScriptObj.RegCustomOpcode((opcode_idx_t)0xA1, TestOpcode);
		
		return;
	}
	
	inline void Loop(uint32_t &time)
	{
		
		
		time = millis();
		return;
	}
};
