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

	static constexpr uint32_t NOR_OFFSET = 8 * 1024 * 1024;
	static constexpr uint16_t NOR_SECTOR_SIZE = SPICore::flash.NOR_SECTOR_SIZE;
	
	DrakeScriptMapping ScriptMapObj;
	DrakeScriptCore ScriptObj(ScriptMapObj);



//uint8_t *script_data = (uint8_t *)heap_caps_malloc(2048*512, MALLOC_CAP_SPIRAM);
uint8_t *script_data = nullptr;

uint8_t *universal_buffer = nullptr;

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






	uint32_t calc_crc_inplace(uint8_t *buff);




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
		uint8_t *nor_buffer = (uint8_t *)heap_caps_malloc(NOR_SECTOR_SIZE, MALLOC_CAP_INTERNAL);								/// заменить на universal_buffer
		uint32_t nor_read_offset;
		nor_script_header_h *nor_script_header = (nor_script_header_h *) nor_buffer;

		uint32_t psram_write_offset = 0;

		for(uint16_t script_id = 0; script_id < SCRIPTS_COUNT; ++script_id)
		{
			memset(nor_buffer, 0x00, NOR_SECTOR_SIZE);
			nor_read_offset = NOR_OFFSET + (NOR_SECTOR_SIZE * script_id);
			
			SPICore::flash.ReadBytes(nor_read_offset, NOR_SECTOR_SIZE, nor_buffer);
			
			if(nor_script_header->length > (NOR_SECTOR_SIZE - sizeof(nor_script_header_h)))
			{
				DEBUG_LOG_TOPIC("PSRAM", "Invalid script length! id:%d, len:%u\n", script_id, nor_script_header->length);
				continue;
			}

			if(nor_script_header->flag1 != 0x55)
			{
				DEBUG_LOG_TOPIC("PSRAM", "Invalid flag1! id:%d\n", script_id);
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

		SPICore::flash.EraseSector(sector);
		SPICore::flash.WaitReady();

		for(uint8_t i = 0; i < page_count; ++i)
		{
			SPICore::flash.WritePage(page + i, &data[i * 256]);
			SPICore::flash.WaitReady();
		}
		
		return;
	}
	
	
	// 
	void DeleteFromSPIFlash(uint16_t script_id)
	{
		if(script_id >= SCRIPTS_COUNT) return;
		
		uint32_t nor_offset = NOR_OFFSET + (NOR_SECTOR_SIZE * script_id);
		uint32_t sector = nor_offset / NOR_SECTOR_SIZE;
		
		SPICore::flash.EraseSector(sector);
		SPICore::flash.WaitReady();
		
		return;
	}


	void GetScriptHeader(uint16_t script_id, nor_script_header_h &header)
	{
		if(script_id >= SCRIPTS_COUNT) return;
		
		uint32_t nor_read_offset = NOR_OFFSET + (NOR_SECTOR_SIZE * script_id);
		// Добавить проверку чтобы нельзя было читать вне границв скрипта и зоны скриптов, через указания неверного script_id
		
		SPICore::flash.ReadBytes(nor_read_offset, sizeof(header), (uint8_t *)&header);
		
		return;
	}






























	// Входящий запрос информации о скрипте
	struct __attribute__((packed)) script_info_req_t
	{
		uint8_t cmd = 0x01;
		uint16_t script_id;
	};
	
	// Исходящий ответ информации о скрипте
	struct __attribute__((packed)) script_info_resp_t
	{
		uint8_t cmd = 0x81;
		uint16_t script_id;
		int8_t state;
	};
	
	
	// Входящий запрос на удаление скрипта
	struct __attribute__((packed)) script_delete_req_t
	{
		uint8_t cmd = 0x02;
		uint16_t script_id;
	};
	
	// Исходящий ответ удаление скрипта
	struct __attribute__((packed)) script_delete_resp_t
	{
		uint8_t cmd = 0x82;
		uint16_t script_id;
	};
	
	
	// Входящий запрос на начало записи скрипта
	struct __attribute__((packed)) script_upload_init_req_t
	{
		uint8_t cmd = 0x03;
		uint16_t script_id;
		uint16_t length;
	};
	
	// Исходящий ответ на начало записи скрипта
	struct __attribute__((packed)) script_upload_init_resp_t
	{
		uint8_t cmd = 0x83;
		uint16_t script_id;
		uint16_t length;
		uint8_t chunk_size;
		uint8_t chunk_nums;
	};


	// Входящий запрос на начало чтения скрипта
	struct __attribute__((packed)) script_download_init_req_t
	{
		uint8_t cmd = 0x04;
		uint16_t script_id;
	};
	
	// Исходящий ответ на начало чтения скрипта
	struct __attribute__((packed)) script_download_init_resp_t
	{
		uint8_t cmd = 0x84;
		uint16_t script_id;
		uint16_t length;
		uint8_t chunk_size;
		uint8_t chunk_nums;
	};





	// Запрос от телефона в Main. Процесс передачи скрипта
	struct __attribute__((packed)) script_upload_proc_req_t
	{
		uint8_t cmd = 0x04;
		uint16_t script_id;
		uint8_t chunk_num;
		uint8_t data[60];
	};

/*
	// Запрос от телефона в Main на завершение передачи скрипта
	struct __attribute__((packed)) script_upload_finish_req_t
	{
		uint8_t cmd = 0x06;
		uint16_t script_id;
	};
*/

	// Запрос от телефона в Main на завершение передачи скрипта
	struct __attribute__((packed)) script_upload_finish_resp_t
	{
		uint8_t cmd = 0x07;
		uint16_t script_id;
		int8_t state;
	};



	// Запрос на перезагрузку всех скриптов
	struct __attribute__((packed)) script_apply_req_t
	{
		uint8_t cmd = 0x08;
	};
	// Запрос на перезагрузку всех скриптов
	struct __attribute__((packed)) script_apply_resp_t
	{
		uint8_t cmd = 0x09;
	};

	
	// Запрос на временное отключение скрипта
	struct __attribute__((packed)) script_ctrl_req_t
	{
		uint8_t cmd = 0x0A;
		uint16_t script_id;
		uint8_t mode;
	};


	// Проверить корректность скрипта в NOR памяти
	struct __attribute__((packed)) script_check_req_t
	{
		uint8_t cmd = 0x0C;
		uint16_t script_id;
		uint16_t length;
		uint32_t crc;
	};
	struct __attribute__((packed)) script_check_resp_t
	{
		uint8_t cmd = 0x0D;
		uint16_t script_id;
		int8_t state;
	};


	bool rx_script_ready = false;
	bool tx_script_ready = false;


	bool L3Rx(L3Wrapper::packet_t &request, L3Wrapper::packet_t &response)
	{
		if(request.GetDataLength() == 0) return false;
		
		uint8_t *data_ptr = request.GetDataPtr();
		uint8_t data_len = request.GetDataLength();
		uint8_t cmd = data_ptr[0];

		bool result = false;
		
		switch(cmd)
		{
			// Запрос и ответ информации о скрипте в NOR памяти.
			// Стоит добавить проверку состояния в PSRAM и флаг en.
			case 0x01:
			{
				if(data_len != sizeof(script_info_req_t)) break;
				
				auto req = (script_info_req_t *) data_ptr;
				
				nor_script_header_h header = {};
				GetScriptHeader(req->script_id, header);

				script_info_resp_t resp = {};
				resp.script_id = req->script_id;
				resp.state = (header.flag1 == 0x55) ? 1 : 0;
				response.Type( request.Type() );
				response.PutData((uint8_t *)&resp, sizeof(resp));

				result = true;
				break;
			}
			
			// Запрос и ответ удаления скрипта
			case 0x02:
			{
				if(data_len != sizeof(script_delete_req_t)) break;
				
				auto req = (script_delete_req_t *) data_ptr;
				
				DeleteFromSPIFlash(req->script_id);

				script_delete_resp_t resp = {};
				resp.script_id = req->script_id;
				response.Type( request.Type() );
				response.PutData((uint8_t *)&resp, sizeof(resp));
				
				result = true;
				break;
			}

			// Запрос и ответ на начало записи скрипта
			case 0x03:
			{
				if(data_len != sizeof(script_upload_init_req_t)) break;
				
				auto req = (script_upload_init_req_t *) data_ptr;
				
				memset(universal_buffer, 0xFF, NOR_SECTOR_SIZE);
				rx_script_ready = true;
				
				const uint8_t chunk_size = 60;
				script_upload_init_resp_t resp = {};
				resp.script_id = req->script_id;
				resp.length = req->length;
				resp.chunk_size = chunk_size;
				resp.chunk_nums = (req->length + (chunk_size - 1)) / chunk_size;
				response.Type( request.Type() );
				response.PutData((uint8_t *)&resp, sizeof(resp));
				
				result = true;
				break;
			}

			// Запрос и ответ на начало чтение скрипта
			case 0x04:
			{
				if(data_len != sizeof(script_download_init_req_t)) break;
				
				auto req = (script_download_init_req_t *) data_ptr;
				
				memset(universal_buffer, 0xFF, NOR_SECTOR_SIZE);
				tx_script_ready = true;
				
				nor_script_header_h header = {};
				GetScriptHeader(req->script_id, header);
				
				const uint8_t chunk_size = 60;
				script_download_init_resp_t resp = {};
				resp.script_id = req->script_id;
				resp.length = header.length;
				resp.chunk_size = chunk_size;
				resp.chunk_nums = (header.length + (chunk_size - 1)) / chunk_size;
				response.Type( request.Type() );
				response.PutData((uint8_t *)&resp, sizeof(resp));
				
				result = true;
				break;
			}
		}
		
		return result;
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
		
		universal_buffer = (uint8_t *)heap_caps_malloc(NOR_SECTOR_SIZE, MALLOC_CAP_INTERNAL);



		script_data = (uint8_t *)heap_caps_malloc(PSRAM_MALLOC_SIZE, MALLOC_CAP_SPIRAM);
		if(!script_data)
		{
			DEBUG_LOG_TOPIC("PSRAM", "PSRAM alloc failed\n");
			return;
		}
		
		ScriptMapObj.SetScriptsArray(script_data, PSRAM_MALLOC_SIZE);
		ScriptObj.RegCustomOpcode((opcode_idx_t)0xA0, TestOpcode);
		ScriptObj.RegCustomOpcode((opcode_idx_t)0xA1, TestOpcode);
		LoadFromSPIFlash();
		
		return;
	}
	
	inline void Loop(uint32_t &time)
	{
		
		
		time = millis();
		return;
	}
};
