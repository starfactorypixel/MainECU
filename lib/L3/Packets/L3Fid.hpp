#pragma once
#include <inttypes.h>
#include <stddef.h>
#include "L3PacketConsts.hpp"

// Идентификаторы сообщений (первый байт каждого сообщения внутри DATA) и транспортные сообщения.
// Запрос и ответ имеют один fId, различаются битом направления в заголовке пакета.
// Сообщения FID_BEACON и FID_PING обрабатывает сам драйвер, наверх (L3Manager) они не передаются.
namespace L3Fid
{
	enum fid_t : uint8_t
	{
		FID_BEACON = 0x00,		// ESP -> все: SN, имя, интервал, состояние сеанса
		FID_PING = 0x01,		// ESP -> клиент при паузе, клиент -> ESP ответ
	};

	// Флаги beacon
	static constexpr uint8_t BEACON_FLAG_SESSION = 0x01;	// у ESP на этом канале открыт сеанс

	// Beacon передаётся без хвоста имени: BEACON_HEAD_LEN + length байт (13..45).
	// Длина сообщения при разборе: BEACON_HEAD_LEN + length
	structpack beacon_t
	{
		uint8_t fId;			// FID_BEACON
		uint16_t interval;		// интервал beacon, мс
		uint8_t flags;			// BEACON_FLAG_*
		uint8_t sn[8];			// серийный номер
		uint8_t length;			// длина имени в байтах, 0..32
		uint8_t name[32];		// имя; в эфир уходят только length байт
	};
	static_assert(sizeof(beacon_t) == 45, "beacon_t size mismatch");

	static constexpr uint8_t BEACON_HEAD_LEN = offsetof(beacon_t, name);	// 13
	static_assert(BEACON_HEAD_LEN == 13, "beacon_t layout mismatch");

	structpack ping_t
	{
		uint8_t fId;			// FID_PING
	};
	static_assert(sizeof(ping_t) == 1, "ping_t size mismatch");
};
