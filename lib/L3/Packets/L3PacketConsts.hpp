#pragma once
#include <inttypes.h>

namespace L3PacketConsts
{

#define structpack		struct __attribute__((packed))

/*
Пакет (транспорт и security объединены), многобайтные поля — little-endian
	[HEAD] - 2 байта сигнатуры 'P','X'
	[CTRL] - 2 байта: байт 2 — версия (биты 0..3) + направление (бит 7), байт 3 — формат (биты 0..2)
	[LEN]  - 2 байта длины PAYL
	[PAYL] - RAW: сырые данные. SIGN/ENCRYPT: [IV 12][DATA 0..64][TAG 16]
	[CRC]  - 2 байта CRC16, сразу за PAYL (позиция зависит от LEN)

IV (12 байт, только SIGN/ENCRYPT)
	[0..2]  epoch    - 24 бита
	[3]     instance - ESP: маска канала (тип драйвера), клиент: её инверсия
	[4..7]  counter  - 32 бита
	[8..11] random   - ESP: новое при каждом запуске, клиент: новое при каждом подключении
	Номер пакета N = epoch<<32 | counter (56 бит). Каждая сторона ведёт свой N и увеличивает его на 1
	на каждый пакет, переполнение counter переходит в epoch. Получатель принимает только N строго больше
	последнего принятого. Эпоха 0xFFFFFF исчерпана навсегда: нужен новый ключ.

Подпись (AAD GCM): SIGN — [HEAD..конец DATA], ENCRYPT — [HEAD..конец IV], данные шифруются.
CRC считается последним и не защищает от подделки, это только фильтр битых кадров.
*/

	static constexpr uint16_t PACKET_HEADER_LEN = 6U;
	static constexpr uint16_t PACKET_IV_LEN = 12U;
	static constexpr uint16_t PACKET_TAG_LEN = 16U;
	static constexpr uint16_t PACKET_DATA_LEN_MAX = 64U;
	static constexpr uint16_t PACKET_PAYLOAD_RAW_LEN = PACKET_IV_LEN + PACKET_DATA_LEN_MAX + PACKET_TAG_LEN;	// 92
	static constexpr uint16_t PACKET_CRC_LEN = 2U;
	static constexpr uint16_t PACKET_LEN_MIN = PACKET_HEADER_LEN + PACKET_CRC_LEN;							// 8
	static constexpr uint16_t PACKET_LEN_MAX = PACKET_HEADER_LEN + PACKET_PAYLOAD_RAW_LEN + PACKET_CRC_LEN;	// 100

	static constexpr uint8_t PACKET_VERSION = 0x02;
	static constexpr uint8_t PACKET_HEAD_0 = 'P';
	static constexpr uint8_t PACKET_HEAD_1 = 'X';

	static constexpr uint32_t SEQ_EPOCH_MAX = 0x00FFFFFFU;				// эпоха 24 бита
	static constexpr uint64_t SEQ_N_MAX = 0x00FFFFFFFFFFFFFFULL;		// N 56 бит

	enum error_t : uint8_t
	{
		ERROR_NONE = 0,

		// Конфигурация и состояние счётчиков
		ERROR_CFG_NO_KEY = 0x01,			// ключ не задан
		ERROR_CFG_NO_SEQ = 0x02,			// не задано состояние счётчиков (L3PacketSeq) или эпоха
		ERROR_CFG_FORMAT = 0x03,			// формат зарезервирован/неизвестен (передача)
		ERROR_SEQ_NOT_STARTED = 0x04,		// L3PacketSeq::Start() не вызван или завершился ошибкой
		ERROR_SEQ_EXHAUSTED = 0x05,			// 24-битная эпоха исчерпана, нужен новый ключ

		// Передача
		ERROR_TX_SEALED = 0x10,				// пакет уже подписан, менять нельзя
		ERROR_TX_DATA_TOO_BIG = 0x11,		// данные не поместятся даже в пустой пакет
		ERROR_TX_NO_SPACE = 0x12,			// в текущем пакете нет места, нужен Commit
		ERROR_TX_CRYPTO = 0x13,				// сбой mbedtls при подписи/шифровании

		// Приём: кадр
		ERROR_RX_FRAME_TOO_SHORT = 0x20,	// кадр короче минимума
		ERROR_RX_FRAME_TOO_LONG = 0x21,		// кадр длиннее максимума
		ERROR_RX_HEAD = 0x22,				// неверная сигнатура
		ERROR_RX_VERSION = 0x23,			// неверная версия
		ERROR_RX_DIRECTION = 0x24,			// неверное направление
		ERROR_RX_PAYLOAD_LEN_MAX = 0x25,	// payload_len больше максимума
		ERROR_RX_LEN_MISMATCH = 0x26,		// принятая длина не равна 6 + payload_len + 2
		ERROR_RX_CRC = 0x27,				// CRC не совпал

		// Приём: формат
		ERROR_RX_FORMAT_RESERVED = 0x30,	// зарезервированный формат
		ERROR_RX_FORMAT_DENIED = 0x31,		// формат запрещён политикой приёма
		ERROR_RX_PAYLOAD_LEN_MIN = 0x32,	// payload_len меньше IV+TAG
		ERROR_RX_DATA_LEN = 0x33,			// данных больше допустимого для формата

		// Приём: безопасность
		ERROR_RX_IV_INSTANCE = 0x40,		// instance в IV не равен ожидаемому
		ERROR_RX_IV_REPLAY = 0x41,			// N не больше последнего принятого
		ERROR_RX_AUTH = 0x42,				// тег GCM не совпал
		ERROR_RX_CRYPTO = 0x43,				// сбой mbedtls (не ошибка аутентификации)

		// Приём: очередь
		ERROR_RX_QUEUE_FULL = 0x50,			// очередь payload переполнена, пакет потерян
	};

	static inline const char *ErrorName(error_t e)
	{
		switch(e)
		{
			case ERROR_NONE:				return "NONE";
			case ERROR_CFG_NO_KEY:			return "CFG_NO_KEY";
			case ERROR_CFG_NO_SEQ:			return "CFG_NO_SEQ";
			case ERROR_CFG_FORMAT:			return "CFG_FORMAT";
			case ERROR_SEQ_NOT_STARTED:		return "SEQ_NOT_STARTED";
			case ERROR_SEQ_EXHAUSTED:		return "SEQ_EXHAUSTED";
			case ERROR_TX_SEALED:			return "TX_SEALED";
			case ERROR_TX_DATA_TOO_BIG:		return "TX_DATA_TOO_BIG";
			case ERROR_TX_NO_SPACE:			return "TX_NO_SPACE";
			case ERROR_TX_CRYPTO:			return "TX_CRYPTO";
			case ERROR_RX_FRAME_TOO_SHORT:	return "RX_FRAME_TOO_SHORT";
			case ERROR_RX_FRAME_TOO_LONG:	return "RX_FRAME_TOO_LONG";
			case ERROR_RX_HEAD:				return "RX_HEAD";
			case ERROR_RX_VERSION:			return "RX_VERSION";
			case ERROR_RX_DIRECTION:		return "RX_DIRECTION";
			case ERROR_RX_PAYLOAD_LEN_MAX:	return "RX_PAYLOAD_LEN_MAX";
			case ERROR_RX_LEN_MISMATCH:		return "RX_LEN_MISMATCH";
			case ERROR_RX_CRC:				return "RX_CRC";
			case ERROR_RX_FORMAT_RESERVED:	return "RX_FORMAT_RESERVED";
			case ERROR_RX_FORMAT_DENIED:	return "RX_FORMAT_DENIED";
			case ERROR_RX_PAYLOAD_LEN_MIN:	return "RX_PAYLOAD_LEN_MIN";
			case ERROR_RX_DATA_LEN:			return "RX_DATA_LEN";
			case ERROR_RX_IV_INSTANCE:		return "RX_IV_INSTANCE";
			case ERROR_RX_IV_REPLAY:		return "RX_IV_REPLAY";
			case ERROR_RX_AUTH:				return "RX_AUTH";
			case ERROR_RX_CRYPTO:			return "RX_CRYPTO";
			case ERROR_RX_QUEUE_FULL:		return "RX_QUEUE_FULL";
			default:						return "UNKNOWN";
		}
	}

	enum direction_t : uint8_t
	{
		DIRECTION_FROM = 0,						// от ESP
		DIRECTION_TO = 1						// к ESP
	};

	// Поле формата — 3 бита (0..7). Коды 4..7 зарезервированы под будущие форматы
	enum format_t : uint8_t
	{
		FORMAT_RAW = 0,							// Данные открыты, не аутентифицированы
		FORMAT_SIGN = 1,						// Данные открыты, весь пакет подписан
		FORMAT_RESERVED = 2,					// Зарезервировано
		FORMAT_ENCRYPT = 3,						// Данные зашифрованы, заголовок+IV подписаны
	};

	static constexpr uint8_t FORMAT_COUNT = 8;	// 2^3, размер таблиц по формату

	// Маска форматов, несущих аутентификацию (бит на формат, 8 форматов — uint8_t)
	static constexpr uint8_t FORMATS_SECURE = (1U << FORMAT_SIGN) | (1U << FORMAT_ENCRYPT);

	// IV: через raw — весь блок для GCM, через поля — части. Платформа одна (GCC, little-endian),
	// раскладка битового поля предсказуема; static_assert ниже ловит, если это когда-то изменится
	structpack IV_t
	{
		union
		{
			structpack
			{
				uint32_t epoch : 24;
				uint8_t instance;		// ESP: маска канала, клиент: её инверсия
				uint32_t counter;
				uint8_t random[4];
			};
			uint8_t raw[PACKET_IV_LEN];
		};

		uint64_t GetN() const
		{
			return ((uint64_t)epoch << 32) | counter;
		}

		void SetN(uint64_t n)
		{
			epoch = (uint32_t)(n >> 32) & SEQ_EPOCH_MAX;
			counter = (uint32_t)n;

			return;
		}
	};
	static_assert(sizeof(IV_t) == PACKET_IV_LEN, "IV_t layout mismatch");

	structpack packet_t
	{
		uint8_t head[2];						// 'P','X'

		uint8_t version : 4;					// Версия протокола
		uint8_t _dummy1 : 3;
		direction_t direction : 1;				// Флаг направления

		format_t format : 3;					// Формат пакета
		uint8_t _dummy2 : 5;

		uint16_t payload_len;					// Длина значимых данных в payload[]
												// (SIGN/ENCRYPT — IV+DATA+TAG, RAW — сырые данные)

		uint8_t payload[PACKET_PAYLOAD_RAW_LEN];

		uint16_t crc;							// Только для sizeof(packet_t)==PACKET_LEN_MAX.
												// Реальная позиция CRC — сразу за фактическими данными,
												// доступ только через L3Packet::_ReadCRC()/_WriteCRC()
	};
};
