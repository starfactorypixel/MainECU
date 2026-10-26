#pragma once
/*
	Протокол скриптов ESP32 ⇄ server.js — сторона ESP. Описание протокола: SCRIPTS_PROTOCOL.md.
	Подключается в конце ScriptLogic.h (нужны его функции флеш-памяти и ScriptMapObj).

	Запрос: [fId 0x1B][cmd][поля…]
	Ответ:  [fId 0x1B][cmd | 0x80][status][поля…]
	Длину сообщения передаёт драйвер L3. Структуры ниже — сообщения целиком, поле данных всегда последнее.

	Источник правды — скрипты, загруженные в PSRAM (то, что исполняется): LIST, INFO, READ, ENABLE.
	WRITE_END и DELETE меняют только флеш-память и ставят reload_needed; в PSRAM изменения попадут после RELOAD.

	Нужно в библиотеке (DrakeScriptMappingInterface):
		bool IsLoaded(uint16_t id)															— скрипт загружен в PSRAM
		bool GetScriptPtr(uint16_t id, uint8_t *&array_ptr, uint16_t &length, bool always = false)	— always: и отключённый

	Вызов из обработчика L3:
		uint8_t tx[ScriptProto::MSG_MAX];
		uint16_t tx_len;
		ScriptProto::Rx(msg, len, tx, tx_len);		// msg[0] — fId
		if(tx_len > 0) { driver->AddPacket(tx, tx_len); driver->Commit(); }

	На любой запрос с cmd есть ответ: при ошибке status < 0, после него — id (если запрос разобран).
*/

namespace ScriptProto
{
	using namespace ScriptLogic;

	static constexpr uint8_t FID = 0x1B;
	static constexpr uint8_t RESP_BIT = 0x80;

	// Сообщение (fId + данные) — не больше DATA L3 минус байт len и не больше 255
	static constexpr uint16_t MSG_MAX = (L3PacketConsts::PACKET_DATA_LEN_MAX - 1 < 255) ? L3PacketConsts::PACKET_DATA_LEN_MAX - 1 : 255;
	static constexpr uint16_t SCRIPT_LEN_MAX = NOR_SECTOR_SIZE - sizeof(nor_script_header_h);

	enum cmd_t : uint8_t
	{
		CMD_INFO        = 0x01,
		CMD_DELETE      = 0x02,
		CMD_WRITE_BEGIN = 0x03,
		CMD_WRITE       = 0x04,
		CMD_WRITE_END   = 0x05,
		CMD_READ        = 0x06,
		CMD_RELOAD      = 0x08,
		CMD_ENABLE      = 0x0A,
		CMD_LIST        = 0x0C,
	};

	enum status_t : int8_t
	{
		ST_OK           =   0,
		ST_BAD_REQUEST  =  -1,	// неизвестная команда или неверная длина запроса
		ST_BAD_ID       =  -2,	// id >= SCRIPTS_COUNT
		ST_NOT_FOUND    =  -3,	// скрипт не загружен в PSRAM
		ST_TOO_LONG     =  -4,	// длина больше SCRIPT_LEN_MAX
		ST_BAD_OFFSET   =  -5,	// кусок с дыркой или за концом скрипта; чтение за концом
		ST_NO_WRITE     =  -6,	// WRITE / WRITE_END без WRITE_BEGIN
		ST_CRC_MISMATCH =  -7,
		ST_INCOMPLETE   =  -8,	// WRITE_END, а принято меньше length
		ST_FLASH_ERROR  =  -9,
	};

	enum flags_t : uint8_t
	{
		FLAG_LOADED  = 0x02,	// загружен в PSRAM
		FLAG_ENABLED = 0x04,
	};

	// ---- Запросы ----------------------------------------------------------------

	struct __attribute__((packed)) req_head_t			{ uint8_t fid; uint8_t cmd; };													// RELOAD
	struct __attribute__((packed)) req_id_t				{ uint8_t fid; uint8_t cmd; uint16_t id; };										// INFO, DELETE
	struct __attribute__((packed)) req_write_begin_t	{ uint8_t fid; uint8_t cmd; uint16_t id; uint16_t length; };
	struct __attribute__((packed)) req_write_t			{ uint8_t fid; uint8_t cmd; uint16_t id; uint16_t offset; uint8_t data[]; };	// data — до конца сообщения
	struct __attribute__((packed)) req_write_end_t		{ uint8_t fid; uint8_t cmd; uint16_t id; uint32_t crc32; };
	struct __attribute__((packed)) req_read_t			{ uint8_t fid; uint8_t cmd; uint16_t id; uint16_t offset; uint16_t max_len; };
	struct __attribute__((packed)) req_enable_t		{ uint8_t fid; uint8_t cmd; uint16_t id; uint8_t on; };
	struct __attribute__((packed)) req_list_t			{ uint8_t fid; uint8_t cmd; uint16_t from; };

	// ---- Ответы -----------------------------------------------------------------

	struct __attribute__((packed)) resp_head_t			{ uint8_t fid; uint8_t cmd; int8_t status; };												// BAD_REQUEST
	struct __attribute__((packed)) resp_id_t			{ uint8_t fid; uint8_t cmd; int8_t status; uint16_t id; };								// DELETE, WRITE_END, ошибки
	struct __attribute__((packed)) resp_info_t			{ uint8_t fid; uint8_t cmd; int8_t status; uint16_t id; uint16_t length; uint32_t crc32; uint8_t flags; };
	struct __attribute__((packed)) resp_write_begin_t	{ uint8_t fid; uint8_t cmd; int8_t status; uint16_t id; uint16_t max_chunk; };
	struct __attribute__((packed)) resp_write_t		{ uint8_t fid; uint8_t cmd; int8_t status; uint16_t id; uint16_t offset; };
	struct __attribute__((packed)) resp_read_t			{ uint8_t fid; uint8_t cmd; int8_t status; uint16_t id; uint16_t offset; uint8_t data[]; };
	struct __attribute__((packed)) resp_reload_t		{ uint8_t fid; uint8_t cmd; int8_t status; uint16_t count; };
	struct __attribute__((packed)) resp_enable_t		{ uint8_t fid; uint8_t cmd; int8_t status; uint16_t id; uint8_t flags; };
	struct __attribute__((packed)) list_entry_t		{ uint16_t id; uint16_t length; uint8_t flags; };
	struct __attribute__((packed)) resp_list_t			{ uint8_t fid; uint8_t cmd; int8_t status; uint8_t reload_needed; uint16_t next; list_entry_t entries[]; };

	// Сколько данных помещается в одно сообщение (поле данных в sizeof не входит)
	static constexpr uint16_t WRITE_CHUNK_MAX = MSG_MAX - sizeof(req_write_t);
	static constexpr uint16_t READ_CHUNK_MAX  = MSG_MAX - sizeof(resp_read_t);
	static constexpr uint16_t LIST_PER_RESP   = (MSG_MAX - sizeof(resp_list_t)) / sizeof(list_entry_t);
	static constexpr uint16_t LIST_END = 0xFFFF;

	// ---- Состояние --------------------------------------------------------------

	// Запись скрипта: данные копятся в universal_buffer после места под заголовок флеш-памяти
	struct
	{
		bool active;
		uint16_t id;
		uint16_t length;
		uint16_t received;	// принято подряд от начала
	} write = {};

	// Флеш-память изменена (WRITE_END, DELETE), а в PSRAM ещё старое — нужен RELOAD
	bool reload_needed = false;

	// Отключённые скрипты, бит на скрипт. Помним сами: RELOAD (ScriptMapObj.ReInit) включает все заново
	uint8_t disabled[SCRIPTS_COUNT / 8] = {};

	bool IsDisabled(uint16_t id)
	{
		return (disabled[id / 8] & (1 << (id % 8))) != 0;
	}

	void SetDisabled(uint16_t id, bool value)
	{
		if(value) disabled[id / 8] |=  (1 << (id % 8));
		else      disabled[id / 8] &= ~(1 << (id % 8));
	}

	// ---- Вспомогательное --------------------------------------------------------

	// Флаги загруженного скрипта; не загружен — 0
	uint8_t Flags(uint16_t id)
	{
		if(!ScriptMapObj.IsLoaded(id)) return 0;

		uint8_t flags = FLAG_LOADED;
		if(!IsDisabled(id)) flags |= FLAG_ENABLED;

		return flags;
	}

	// Ответ фиксированной длины: объект целиком в tx
	template<typename T>
	void Answer(uint8_t *tx, uint16_t &tx_len, const T &resp)
	{
		memcpy(tx, &resp, sizeof(resp));
		tx_len = sizeof(resp);
	}

	// ---- Разбор сообщения -------------------------------------------------------
	// msg — сообщение Script (msg[0] — fId), len — его длина от драйвера L3.
	// tx — буфер ответа (MSG_MAX байт), tx_len — длина ответа (0 — ответа нет: в сообщении нет даже cmd)
	void Rx(const uint8_t *msg, uint16_t len, uint8_t *tx, uint16_t &tx_len)
	{
		tx_len = 0;
		if(len < sizeof(req_head_t)) return;

		const uint8_t cmd = ((const req_head_t *) msg)->cmd;
		const uint8_t answer_cmd = cmd | RESP_BIT;

		switch(cmd)
		{
			// INFO: id → id, length, crc32, flags. Из PSRAM; CRC — по тем же байтам, что отдаст READ
			case CMD_INFO:
			{
				if(len != sizeof(req_id_t)) { Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST }); break; }
				const req_id_t *req = (const req_id_t *) msg;

				uint8_t *script;
				uint16_t length;
				if(req->id >= SCRIPTS_COUNT) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_BAD_ID, req->id }); break; }
				if(!ScriptMapObj.IsLoaded(req->id) || !ScriptMapObj.GetScriptPtr(req->id, script, length, true)) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_NOT_FOUND, req->id }); break; }

				resp_info_t resp = {};
				resp.fid = FID;
				resp.cmd = answer_cmd;
				resp.status = ST_OK;
				resp.id = req->id;
				resp.length = length;
				resp.crc32 = esp_crc32_le(0, script, length);
				resp.flags = Flags(req->id);
				Answer(tx, tx_len, resp);
				break;
			}

			// DELETE: id → id. Стирает сектор во флеш-памяти; в PSRAM скрипт остаётся до RELOAD
			case CMD_DELETE:
			{
				if(len != sizeof(req_id_t)) { Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST }); break; }
				const req_id_t *req = (const req_id_t *) msg;

				if(req->id >= SCRIPTS_COUNT) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_BAD_ID, req->id }); break; }

				DeleteFromSPIFlash(req->id);
				reload_needed = true;

				Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_OK, req->id });
				break;
			}

			// WRITE_BEGIN: id, length → id, max_chunk. Новая запись отменяет незаконченную
			case CMD_WRITE_BEGIN:
			{
				if(len != sizeof(req_write_begin_t)) { Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST }); break; }
				const req_write_begin_t *req = (const req_write_begin_t *) msg;

				if(req->id >= SCRIPTS_COUNT) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_BAD_ID, req->id }); break; }
				if(req->length == 0 || req->length > SCRIPT_LEN_MAX) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_TOO_LONG, req->id }); break; }

				memset(universal_buffer, 0xFF, NOR_SECTOR_SIZE);	// SaveToSPIFlash ждёт 0xFF в незаполненном
				write.active = true;
				write.id = req->id;
				write.length = req->length;
				write.received = 0;

				resp_write_begin_t resp = {};
				resp.fid = FID;
				resp.cmd = answer_cmd;
				resp.status = ST_OK;
				resp.id = req->id;
				resp.max_chunk = WRITE_CHUNK_MAX;
				Answer(tx, tx_len, resp);
				break;
			}

			// WRITE: id, offset, data → id, offset. Кусок можно прислать повторно; дырка (offset больше принятого) — ошибка
			case CMD_WRITE:
			{
				if(len <= sizeof(req_write_t)) { Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST }); break; }
				const req_write_t *req = (const req_write_t *) msg;
				const uint16_t data_len = len - sizeof(req_write_t);

				resp_write_t resp = {};
				resp.fid = FID;
				resp.cmd = answer_cmd;
				resp.status = ST_OK;
				resp.id = req->id;
				resp.offset = req->offset;

				if(!write.active || write.id != req->id)
				{
					resp.status = ST_NO_WRITE;
				}
				else if(req->offset > write.received || req->offset + data_len > write.length)
				{
					resp.status = ST_BAD_OFFSET;
				}
				else
				{
					memcpy(&universal_buffer[sizeof(nor_script_header_h) + req->offset], req->data, data_len);
					if(req->offset + data_len > write.received) write.received = req->offset + data_len;
				}

				Answer(tx, tx_len, resp);
				break;
			}

			// WRITE_END: id, crc32 → id. Проверить, что принято всё и CRC совпал, записать во флеш-память
			case CMD_WRITE_END:
			{
				if(len != sizeof(req_write_end_t)) { Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST }); break; }
				const req_write_end_t *req = (const req_write_end_t *) msg;

				if(!write.active || write.id != req->id) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_NO_WRITE, req->id }); break; }
				if(write.received != write.length) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_INCOMPLETE, req->id }); break; }

				uint32_t crc = esp_crc32_le(0, &universal_buffer[sizeof(nor_script_header_h)], write.length);
				if(crc != req->crc32) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_CRC_MISMATCH, req->id }); break; }

				write.active = false;
				SaveToSPIFlash(req->id, universal_buffer, write.length);
				reload_needed = true;

				// Проверка: заголовок записался
				nor_script_header_h header = {};
				GetScriptHeader(req->id, header);
				int8_t status = (header.flag1 == 0x55 && header.length == write.length) ? ST_OK : ST_FLASH_ERROR;

				Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, status, req->id });
				break;
			}

			// READ: id, offset, max_len → id, offset, data. Из PSRAM
			case CMD_READ:
			{
				if(len != sizeof(req_read_t)) { Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST }); break; }
				const req_read_t *req = (const req_read_t *) msg;

				uint8_t *script;
				uint16_t length;
				if(req->id >= SCRIPTS_COUNT) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_BAD_ID, req->id }); break; }
				if(!ScriptMapObj.IsLoaded(req->id) || !ScriptMapObj.GetScriptPtr(req->id, script, length, true)) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_NOT_FOUND, req->id }); break; }
				if(req->offset >= length) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_BAD_OFFSET, req->id }); break; }

				// Сколько отдать: остаток, но не больше, чем просит сервер и помещается в наш ответ
				uint16_t n = length - req->offset;
				if(n > req->max_len) n = req->max_len;
				if(n > READ_CHUNK_MAX) n = READ_CHUNK_MAX;

				// Данные переменной длины — заполняем прямо в tx
				resp_read_t *resp = (resp_read_t *) tx;
				resp->fid = FID;
				resp->cmd = answer_cmd;
				resp->status = ST_OK;
				resp->id = req->id;
				resp->offset = req->offset;
				memcpy(resp->data, &script[req->offset], n);
				tx_len = sizeof(resp_read_t) + n;
				break;
			}

			// RELOAD: — → count. Загрузить все скрипты из флеш-памяти в PSRAM; отключённые остаются отключёнными
			case CMD_RELOAD:
			{
				if(len != sizeof(req_head_t)) { Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST }); break; }

				write.active = false;	// LoadFromSPIFlash использует universal_buffer

				resp_reload_t resp = {};
				resp.fid = FID;
				resp.cmd = answer_cmd;
				resp.status = ST_OK;
				resp.count = LoadFromSPIFlash();
				reload_needed = false;

				for(uint16_t id = 0; id < SCRIPTS_COUNT; ++id)
				{
					if(IsDisabled(id)) ScriptMapObj.DisableScript(id);
				}

				Answer(tx, tx_len, resp);
				break;
			}

			// ENABLE: id, on → id, flags. До перезапуска ESP
			case CMD_ENABLE:
			{
				if(len != sizeof(req_enable_t)) { Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST }); break; }
				const req_enable_t *req = (const req_enable_t *) msg;

				if(req->id >= SCRIPTS_COUNT) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_BAD_ID, req->id }); break; }
				if(!ScriptMapObj.IsLoaded(req->id)) { Answer(tx, tx_len, resp_id_t{ FID, answer_cmd, ST_NOT_FOUND, req->id }); break; }

				if(req->on) ScriptMapObj.EnableScript(req->id);
				else        ScriptMapObj.DisableScript(req->id);
				SetDisabled(req->id, req->on == 0);

				resp_enable_t resp = {};
				resp.fid = FID;
				resp.cmd = answer_cmd;
				resp.status = ST_OK;
				resp.id = req->id;
				resp.flags = Flags(req->id);
				Answer(tx, tx_len, resp);
				break;
			}

			// LIST: from → reload_needed, next, entries[]. Загруженные скрипты с id >= from;
			// next = 0xFFFF — конец списка, иначе сервер спросит с next
			case CMD_LIST:
			{
				if(len != sizeof(req_list_t)) { Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST }); break; }
				const req_list_t *req = (const req_list_t *) msg;

				// Записи переменной длины — заполняем прямо в tx
				resp_list_t *resp = (resp_list_t *) tx;
				resp->fid = FID;
				resp->cmd = answer_cmd;
				resp->status = ST_OK;
				resp->reload_needed = reload_needed;
				resp->next = LIST_END;

				uint16_t count = 0;
				for(uint16_t id = req->from; id < SCRIPTS_COUNT; ++id)
				{
					uint8_t *script;
					uint16_t length;
					if(!ScriptMapObj.IsLoaded(id) || !ScriptMapObj.GetScriptPtr(id, script, length, true)) continue;

					if(count == LIST_PER_RESP) { resp->next = id; break; }	// не помещается — продолжить с него

					resp->entries[count].id = id;
					resp->entries[count].length = length;
					resp->entries[count].flags = Flags(id);
					count++;
				}

				tx_len = sizeof(resp_list_t) + count * sizeof(list_entry_t);
				break;
			}

			default:
			{
				Answer(tx, tx_len, resp_head_t{ FID, answer_cmd, ST_BAD_REQUEST });
				break;
			}
		}
	}
};
