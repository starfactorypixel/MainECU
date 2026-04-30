#pragma once
#include <inttypes.h>
#include <string.h>
#include "esp_crc.h"
#include <DrakeScriptCore.hpp>
#include <DrakeScriptMappingESP32PSRAM.hpp>
#include "ScriptLogicCustom.h"

namespace ScriptLogic
{
	static constexpr uint16_t SCRIPTS_COUNT = 2048;
	static constexpr uint32_t PSRAM_MALLOC_SIZE = SCRIPTS_COUNT * 512;

	static constexpr uint32_t NOR_OFFSET = 8 * 1024 * 1024;
	static constexpr uint16_t NOR_SECTOR_SIZE = SPICore::flash.NOR_SECTOR_SIZE;
	
	DrakeScriptMappingESP32PSRAM<PSRAM_MALLOC_SIZE> ScriptMapObj;
	DrakeScriptCore ScriptObj(ScriptMapObj);




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
		uint8_t *nor_buffer = universal_buffer;
		uint32_t nor_read_offset = 0;
		nor_script_header_h *nor_script_header = (nor_script_header_h *) nor_buffer;

		for(uint16_t script_id = 0; script_id < SCRIPTS_COUNT; ++script_id)
		{
			memset(nor_buffer, 0x00, NOR_SECTOR_SIZE);
			nor_read_offset = NOR_OFFSET + (NOR_SECTOR_SIZE * script_id);
			
			SPICore::flash.ReadBytes(nor_read_offset, nor_buffer, NOR_SECTOR_SIZE);

			if(nor_script_header->length == 0xFFFF)
			{
				continue;
			}
			
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
			
			bool add_result = ScriptMapObj.AddScript(script_id, &nor_buffer[sizeof(nor_script_header_h)], nor_script_header->length);
			if(add_result == false)
			{
				DEBUG_LOG_TOPIC("PSRAM", "Mo more PSRAM memory! id:%d\n", script_id);
				break;
			}
		}
		
		return;
	}
	
	// data со сдвигом sizeof(nor_script_header_h), length без учёта sizeof(nor_script_header_h)
	// Массив data должен быть заполнен 0xFF по умолчанию.
	void SaveToSPIFlash(uint16_t script_id, uint8_t *data, uint16_t length)
	{
		if(script_id >= SCRIPTS_COUNT) return;
		if(length + sizeof(nor_script_header_h) > NOR_SECTOR_SIZE) return;
		//if(length < sizeof(nor_script_header_h)) return;
		
		nor_script_header_h *header = (nor_script_header_h *) &data[0];
		header->flag1 = 0x55;
		header->flag2 = 0x00;
		header->length = length;
		header->crc32 = calc_crc_inplace(data);
		length += sizeof(nor_script_header_h);
		
		uint32_t nor_offset = NOR_OFFSET + (NOR_SECTOR_SIZE * script_id);
		uint32_t sector = nor_offset / NOR_SECTOR_SIZE;
		uint32_t page = nor_offset / 256;
		uint8_t page_count = (length + 255) / 256;

		SPICore::flash.EraseSector(sector);
		SPICore::flash.WaitReady();

		for(uint8_t i = 0; i < page_count; ++i)
		{
			uint32_t len = (length > 256) ? 256 : length;
			SPICore::flash.WritePage(page + i, &data[i * 256], len);
			SPICore::flash.WaitReady();
			length -= 256;
		}
		
		return;
	}

	void LoadFromSPIFlash(uint16_t script_id)
	{
		uint32_t nor_read_offset = NOR_OFFSET + (NOR_SECTOR_SIZE * script_id);
		
		memset(universal_buffer, 0x00, NOR_SECTOR_SIZE);
		SPICore::flash.ReadBytes(nor_read_offset, universal_buffer, NOR_SECTOR_SIZE);
		
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
		
		SPICore::flash.ReadBytes(nor_read_offset, (uint8_t *)&header, sizeof(header));
		
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
		int8_t state;
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
	
	// Входящий запрос на записи скрипта
	struct __attribute__((packed)) script_upload_proc_req_t
	{
		uint8_t cmd = 0x04;
		uint16_t script_id;
		uint8_t chunk_idx;
		uint8_t data[60];
	};
	
	// Исходящий ответ на записи скрипта
	struct __attribute__((packed)) script_upload_proc_resp_t
	{
		uint8_t cmd = 0x84;
		uint16_t script_id;
		uint8_t chunk_idx;
		int8_t state;
	};
	
	
	// Входящий запрос на начало чтения скрипта
	struct __attribute__((packed)) script_download_init_req_t
	{
		uint8_t cmd = 0x06;
		uint16_t script_id;
	};
	
	// Исходящий ответ на начало чтения скрипта
	struct __attribute__((packed)) script_download_init_resp_t
	{
		uint8_t cmd = 0x86;
		uint16_t script_id;
		uint16_t length;
		uint8_t chunk_size;
		uint8_t chunk_nums;
	};
	
	// Входящий запрос на чтения скрипта
	struct __attribute__((packed)) script_download_proc_req_t
	{
		uint8_t cmd = 0x07;
		uint16_t script_id;
		uint8_t chunk_idx;
	};
	
	// Исходящий ответ на чтения скрипта
	struct __attribute__((packed)) script_download_proc_resp_t
	{
		uint8_t cmd = 0x87;
		uint16_t script_id;
		uint8_t chunk_idx;
		uint8_t data[60];
	};


	// Входящий запрос на перезагрузку всех скриптов
	struct __attribute__((packed)) script_apply_req_t
	{
		uint8_t cmd = 0x08;
	};
	// Исходящий ответ на перезагрузку всех скриптов
	struct __attribute__((packed)) script_apply_resp_t
	{
		uint8_t cmd = 0x09;
		int8_t state;
	};

	
	// Входящий запрос на временное отключение скрипта
	struct __attribute__((packed)) script_ctrl_req_t
	{
		uint8_t cmd = 0x0A;
		uint16_t script_id;
		int8_t state;
	};

	// Исходящий ответ на временное отключение скрипта
	struct __attribute__((packed)) script_ctrl_resp_t
	{
		uint8_t cmd = 0x0A;
		uint16_t script_id;
		int8_t state;
	};


	// Входящий запрос на проверку всех скриптов в NOR памяти
	struct __attribute__((packed)) script_check_req_t
	{
		uint8_t cmd = 0x0C;
	};

	// Исходящий ответ на проверку всех скриптов в NOR памяти
	struct __attribute__((packed)) script_check_resp_t
	{
		uint8_t cmd = 0x0D;
		uint16_t script_id;
		int8_t state;
	};


	static constexpr uint16_t CHUNK_SIZE = 60;

	enum transfer_type_t : uint8_t {TRANSFER_NONE, TRANSFER_UPLOAD, TRANSFER_DOWNLOAD};
	struct transfer_data_t
	{
		transfer_type_t type;			// Тип передачи
		uint16_t id;					// ID скрипта
		uint16_t total_length;			// Общая длина скрипа в байтах
		uint16_t current_length;		// Текущая полученная или переданная длина
	} transfer_data = {};




	bool L3Rx(L3Wrapper::packet_t &request, L3Wrapper::packet_t &response)
	{
		if(request.GetDataLength() == 0) return false;
		
		uint8_t *rx_data_ptr = request.GetDataPtr();
		uint8_t rx_data_len = request.GetDataLength();
		uint8_t rx_cmd = rx_data_ptr[0];

		bool result = false;
		
		switch(rx_cmd)
		{
			// Запрос и ответ информации о скрипте в NOR памяти.
			// Стоит добавить проверку состояния в PSRAM и флаг en.
			case 0x01:
			{
				if(rx_data_len != sizeof(script_info_req_t)) break;
				
				auto req = (script_info_req_t *) rx_data_ptr;
				
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
				if(rx_data_len != sizeof(script_delete_req_t)) break;
				
				auto req = (script_delete_req_t *) rx_data_ptr;
				
				DeleteFromSPIFlash(req->script_id);

				script_delete_resp_t resp = {};
				resp.script_id = req->script_id;
				resp.state = 1;
				response.Type( request.Type() );
				response.PutData((uint8_t *)&resp, sizeof(resp));
				
				result = true;
				break;
			}

			// Запрос и ответ на начало записи скрипта
			case 0x03:
			{
				if(rx_data_len != sizeof(script_upload_init_req_t)) break;
				
				auto req = (script_upload_init_req_t *) rx_data_ptr;
				
				memset(universal_buffer, 0xFF, NOR_SECTOR_SIZE);
				
				transfer_data.type = TRANSFER_UPLOAD;
				transfer_data.id = req->script_id;
				transfer_data.total_length = req->length;
				transfer_data.current_length = 0;			// возможно хранить не 2 знач. а остаток скока байт передать нужною.
				
				script_upload_init_resp_t resp = {};
				resp.script_id = req->script_id;
				resp.length = req->length;
				resp.chunk_size = CHUNK_SIZE;
				resp.chunk_nums = (req->length + (CHUNK_SIZE - 1)) / CHUNK_SIZE;
				response.Type( request.Type() );
				response.PutData((uint8_t *)&resp, sizeof(resp));
				
				result = true;
				break;
			}
			
			// Приём тела скрипта и ответ подтвержением
			case 0x04:
			{
				if(rx_data_len > sizeof(script_upload_proc_req_t)) break;
				if(transfer_data.type != TRANSFER_UPLOAD) break;
				
				auto req = (script_upload_proc_req_t *) rx_data_ptr;
				
				if(transfer_data.id != req->script_id) break;
				
				uint16_t write_buff_offset = (CHUNK_SIZE * req->chunk_idx) + sizeof(nor_script_header_h);
				uint16_t len_left = transfer_data.total_length - transfer_data.current_length;
				uint16_t data_len = (len_left > CHUNK_SIZE) ? CHUNK_SIZE : len_left;

				if(write_buff_offset + data_len > NOR_SECTOR_SIZE) break;
				
				memcpy(&universal_buffer[write_buff_offset], req->data, data_len);
				transfer_data.current_length += data_len;
				
				script_upload_proc_resp_t resp = {};
				resp.script_id = req->script_id;
				resp.chunk_idx = req->chunk_idx;

				if(transfer_data.current_length == transfer_data.total_length)
				{
					SaveToSPIFlash(req->script_id, universal_buffer, transfer_data.total_length);
					resp.state = 2;
				}
				else
				{
					resp.state = 1;
				}
				
				response.Type( request.Type() );
				response.PutData((uint8_t *)&resp, sizeof(resp));
				
				result = true;
				break;
			}


			// Запрос и ответ на начало чтение скрипта
			case 0x06:
			{
				if(rx_data_len != sizeof(script_download_init_req_t)) break;
				
				auto req = (script_download_init_req_t *) rx_data_ptr;

				nor_script_header_h header = {};
				GetScriptHeader(req->script_id, header);
				
				LoadFromSPIFlash(req->script_id);
				transfer_data.type = TRANSFER_DOWNLOAD;
				transfer_data.id = req->script_id;
				transfer_data.total_length = header.length;
				transfer_data.current_length = 0;

				script_download_init_resp_t resp = {};
				resp.script_id = req->script_id;
				resp.length = header.length;
				resp.chunk_size = CHUNK_SIZE;
				resp.chunk_nums = (header.length + (CHUNK_SIZE - 1)) / CHUNK_SIZE;
				response.Type( request.Type() );
				response.PutData((uint8_t *)&resp, sizeof(resp));
				
				result = true;
				break;
			}

			// Отправка тела скрипта
			case 0x07:
			{
				if(rx_data_len > sizeof(script_download_proc_req_t)) break;
				if(transfer_data.type != TRANSFER_DOWNLOAD) break;
				
				auto req = (script_download_proc_req_t *) rx_data_ptr;
				
				if(transfer_data.id != req->script_id) break;

				uint16_t read_buff_offset = (CHUNK_SIZE * req->chunk_idx) + sizeof(nor_script_header_h);
				uint16_t len_left = transfer_data.total_length - transfer_data.current_length;
				uint16_t data_len = (len_left > CHUNK_SIZE) ? CHUNK_SIZE : len_left;

				script_download_proc_resp_t resp = {};
				resp.script_id = req->script_id;
				resp.chunk_idx = req->chunk_idx;
				memcpy(resp.data, &universal_buffer[read_buff_offset], data_len);
				transfer_data.current_length += data_len;
				response.Type( request.Type() );
				response.PutData((uint8_t *)&resp, (data_len + 4));

				result = true;
				break;
			}
		}
		
		return result;
	}







	
	uint32_t calc_crc_inplace(uint8_t *buff)
	{
		nor_script_header_h *hdr = (nor_script_header_h *) buff;
		
		uint32_t old_crc = hdr->crc32;
		hdr->crc32 = 0x00000000;
		uint32_t new_crc = esp_crc32_le(0xFFFFFFFF, buff, (sizeof(nor_script_header_h) + hdr->length))/* ^ 0xFFFFFFFF*/;
		hdr->crc32 = old_crc;
		
		return new_crc;
	}
	
	
	inline void Setup()
	{
		
		universal_buffer = (uint8_t *)heap_caps_malloc(NOR_SECTOR_SIZE, MALLOC_CAP_INTERNAL);

		if(!ScriptMapObj.Init())
		{
			DEBUG_LOG_TOPIC("PSRAM", "PSRAM alloc failed\n");
			return;
		}
		
		ScriptObj.RegCustomOpcode((opcode_idx_t)0x25, TestOpcode);
		ScriptObj.RegCustomOpcode((opcode_idx_t)0x26, TestOpcode);
		LoadFromSPIFlash();
		
		return;
	}
	
	inline void Loop(uint32_t &time)
	{
		
		
		time = millis();
		return;
	}
};
