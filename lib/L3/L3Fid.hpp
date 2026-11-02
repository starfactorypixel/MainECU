#pragma once
#include <inttypes.h>
#include <string.h>
#include "Packets/L3PacketConsts.hpp"

// Идентификаторы сообщений и транспортные сообщения. В DATA перед каждым сообщением идёт его длина:
// [len u8][fId][данные…], len = fId + данные. Структуры ниже — само сообщение, без len.
// Запрос и ответ имеют один fId, различаются битом направления в заголовке пакета.
// Сообщения FID_BEACON, FID_ACCEPT и FID_PING обрабатывает сам драйвер, наверх (L3Manager) они не передаются.
namespace L3Fid
{
	enum fid_t : uint8_t
	{
		FID_BEACON = 0x00,		// ESP -> все: SN, имя, интервал (идентификатор сеанса — в IV)
		FID_ACCEPT = 0x01,		// клиент -> ESP после beacon: открыть сеанс; ESP -> клиент ответ: сеанс открыт
		FID_PING = 0x05,		// ESP -> клиент при паузе, клиент -> ESP ответ

		FID_SUBSCRIBE = 0x12,	// Клиент -> ESP: Запрос на подписку/отписку ID

		FID_SCRIPT_CTRL = 0x1B,	// ESP <-> Клиент: Пакеты управления системой скриптов

		FID_BLOCK_INFO = 0x1D,	// ESP -> Клиент: Пакет информации о блоке
		FID_BLOCK_ERR = 0x1E,	// ESP -> Клиент: Пакет ошибки из блока

		FID_CAN_MSG = 0x35,		// ESP <-> Клиент: Пакеты Из или В CAN
	};


	// Beacon передаётся без хвоста имени: BEACON_HEAD_LEN + length байт (12..44)
	structpack beacon_t
	{
		uint8_t fId;			// FID_BEACON
		uint16_t interval;		// интервал beacon, мс
		uint8_t sn[8];			// серийный номер
		uint8_t length;			// длина имени в байтах, 0..32
		uint8_t name[32];		// имя; в эфир уходят только length байт
	};
	static_assert(sizeof(beacon_t) == 44, "beacon_t size mismatch");

	static constexpr uint8_t BEACON_HEAD_LEN = offsetof(beacon_t, name);
	static_assert(BEACON_HEAD_LEN == 12, "beacon_t layout mismatch");


	structpack accept_t
	{
		uint8_t fId;			// FID_ACCEPT
	};
	static_assert(sizeof(accept_t) == 1, "accept_t size mismatch");


	structpack ping_t
	{
		uint8_t fId;			// FID_PING
	};
	static_assert(sizeof(ping_t) == 1, "ping_t size mismatch");
	
	
	// Наибольшее сообщение (fId + данные): min(PACKET_DATA_LEN_MAX − 1, 255) — длина сообщения в DATA один байт
	static constexpr uint16_t MSG_MAX = (L3PacketConsts::PACKET_DATA_LEN_MAX - 1 > 255) ? 255 : L3PacketConsts::PACKET_DATA_LEN_MAX - 1;

	// Подписка/отписка: [0x12][count][id u16 × count]. id: младшие 15 бит — CAN ID, старший бит (0x8000) — отписка.
	// Пример: отписка 0x0164 -> байты 64 81. В эфир уходит 2 + 2 × count байт, не вся структура
	static constexpr uint8_t max_subscribe_count = (MSG_MAX - 2) / 2;
	structpack subscribe_t
	{
		uint8_t fId = FID_SUBSCRIBE;
		uint8_t count;
		structpack
		{
			uint16_t can_id : 15;
			uint16_t unsubscribe : 1;
		} ids[max_subscribe_count];
	};
	static_assert(sizeof(subscribe_t) <= MSG_MAX, "subscribe_t size mismatch");
	

	// Пакет информации о блоке.
	structpack block_info_t
	{
		uint8_t fId = L3Fid::FID_BLOCK_INFO;
		uint16_t baseId;				// Базовый ID блока

		// Структура должна полностью повторять пакет block_info_static_t из L2 за исключением fId и idx
		structpack
		{
			uint8_t hw_type;			// Тип платы
			uint8_t hw_ver;				// Версия платы
			uint8_t sw_ver;				// Версия программы
			uint8_t can_ver;			// Версия протокола CAN
			uint8_t sn[8];				// Серийный номер блока
			uint8_t features[7];		// Возможности блока
		} istatic;
		
		// Структура должна полностью повторять пакет block_info_dynamic_t из L2 за исключением fId и idx
		structpack
		{
			uint32_t uptime;			// Uptime блока, мс.
			uint16_t voltage;			// Напряжение питание блока
			uint16_t current;			// Общий потребляемый ток блока
			int8_t temperature;			// Температура блока, если есть
			uint8_t error_flags;		// Флаги наличия ошибок блока
		} idynamic;
	};
	static_assert(sizeof(block_info_t) == 32, "block_info_t size mismatch");


	// Пакет ошибки от блока
	structpack block_error_t
	{
		uint8_t fId = L3Fid::FID_BLOCK_ERR;
		uint16_t baseId;				// Базовый ID блока
		uint16_t realId;				// Истинный ID который сообщил о ошибке
		
		// Структура должна полностью повторять пакет block_error_t из L2 за исключением fId
		structpack
		{
			uint8_t group;				// Группа ошибки
			uint8_t code;				// Код ошибки
			uint16_t subcode;			// Доп. код ошибки
		} error;
	};
	static_assert(sizeof(block_error_t) == 9, "block_error_t size mismatch");
	
	
	structpack can_msg_t
	{
		uint8_t fId = FID_CAN_MSG;
		uint16_t can_id;
		uint8_t length;
		uint8_t data[8];

		void SetData(const uint8_t *ptr, uint8_t len)
		{
			if(len > sizeof(data)) len = sizeof(data);
			memcpy(data, ptr, len); length = len;
			return;
		}
		
		uint16_t GetLen()
		{
			return (length + 4);
		}
	};
	static_assert(sizeof(can_msg_t) == 12, "can_msg_t size mismatch");


};
