#pragma once
#include <inttypes.h>
#include <stddef.h>
#include "L3PacketConsts.hpp"

// Идентификаторы сообщений (первый байт каждого сообщения внутри DATA) и транспортные сообщения.
// Запрос и ответ имеют один fId, различаются битом направления в заголовке пакета.
// Сообщения FID_BEACON, FID_ACCEPT и FID_PING обрабатывает сам драйвер, наверх (L3Manager) они не передаются.
namespace L3Fid
{
	enum fid_t : uint8_t
	{
		FID_BEACON = 0x00,		// ESP -> все: SN, имя, интервал (идентификатор сеанса — в IV)
		FID_ACCEPT = 0x01,		// клиент -> ESP после beacon: открыть сеанс; ESP -> клиент ответ: сеанс открыт
		FID_PING = 0x05,		// ESP -> клиент при паузе, клиент -> ESP ответ
	};

	// Beacon передаётся без хвоста имени: BEACON_HEAD_LEN + length байт (12..44).
	// Длина сообщения при разборе: BEACON_HEAD_LEN + length
	structpack beacon_t
	{
		uint8_t fId;			// FID_BEACON
		uint16_t interval;		// интервал beacon, мс
		uint8_t sn[8];			// серийный номер
		uint8_t length;			// длина имени в байтах, 0..32
		uint8_t name[32];		// имя; в эфир уходят только length байт
	};
	static_assert(sizeof(beacon_t) == 44, "beacon_t size mismatch");

	static constexpr uint8_t BEACON_HEAD_LEN = offsetof(beacon_t, name);	// 12
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
};
