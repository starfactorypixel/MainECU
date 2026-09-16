#pragma once
#include <inttypes.h>

namespace L3PacketConsts
{

#define structpack		struct __attribute__((packed))

/*

Transport
	[HEAD] - 2 байта сигнатуры
	[VER]  - 1 байт версии и параметров
	[CTRL] - 1 байт параметров
	[CRC]  - 2 байта CRC16
	[LEN]  - 2 байта длины PAYL
	[PAYL] - от 0 байт данных

Security
	[FORM] - 1 байт формата и параметров
	[CTRL] - 1 байт параметров
	[LEN]  - 2 байта длины PAYL
	[PAYL] - от 0 байт данных
	Если режим RAW, то передём просто сырой пакет данных
		[RAW] - Сырые данные
	
	Если режим SIGN или ENCRYPT то передаём IV DATA TAG
		[IV]   - Вектор инициализации
		[DATA] - Данные открытые или зашифрованные
		[TAG]  - Тег аутентификации

*/

	static constexpr uint16_t SECURITY_IV_LEN = 12U;
	static constexpr uint16_t SECURITY_TAG_LEN = 16U;
	static constexpr uint16_t SECURITY_PAYLOAD_LEN = 64U;
	static constexpr uint16_t SECURITY_PAYLOAD_RAW_LEN = SECURITY_IV_LEN + SECURITY_PAYLOAD_LEN + SECURITY_TAG_LEN;
	static constexpr uint16_t SECURITY_PACKET_LEN_MIN = 4U;
	static constexpr uint16_t SECURITY_PACKET_LEN_MAX = SECURITY_PACKET_LEN_MIN + SECURITY_PAYLOAD_RAW_LEN;
	
	static constexpr uint16_t TRANSPORT_PAYLOAD_LEN_MAX = SECURITY_PACKET_LEN_MAX;
	static constexpr uint16_t TRANSPORT_PACKET_LEN_MIN = 8U;
	static constexpr uint16_t TRANSPORT_PACKET_LEN_MAX = TRANSPORT_PACKET_LEN_MIN + SECURITY_PACKET_LEN_MAX;
	
	static constexpr uint8_t PACKET_VERSION = 0x02;
	static constexpr uint16_t CRC_DEFAULT_VALUE = 0x0000;

	enum error_t : int8_t
	{
		ERROR_NONE = 0,
		ERROR_FORMAT = -1,
		ERROR_DIRECTION = -2,
		ERROR_VERSION = -3,
		ERROR_CRC = -4,
		ERROR_OVERFLOW = -5,
		ERROR_TIMEOUT = -6,
		ERROR_PAYLOAD_LEN = -7,
		ERROR_LEN = -8,
		ERROR_HEAD = -9,
		ERROR_MIN_LEN = -10,
		ERROR_AUTH_FAILED = -11,
	};
	
	enum direction_t : uint8_t
	{
		DIRECTION_FROM = 0,
		DIRECTION_TO = 1
	};
	
	enum format_t : uint8_t
	{
		FORMAT_RAW = 0,							// Данные открыты а весь payload доступен
		FORMAT_SIGN = 1,						// Данные открыты а весь пакет подписан
		FORMAT_RESERVED = 2,					// Зарезервировано
		FORMAT_ENCRYPT = 3,						// Данные зашифрованы а весь пакет подписан
	};
	
	structpack transport_t
	{
		uint8_t head[2];						// Заголовок
		
		uint8_t version : 4;					// Версия протокола: 0x02
		uint8_t _dummy1 : 4;
		
		uint8_t _dummy2 : 7;
		direction_t direction : 1;				// Флаг направления
		
		uint16_t crc;							// CRC16_XModem
		
		uint16_t payload_len;					// Длина payload
		uint8_t payload[TRANSPORT_PAYLOAD_LEN_MAX];	// Данные
	};

	structpack security_t
	{
		format_t format : 2;					// Формат пакета
		uint8_t _dummy1 : 6;
		
		uint8_t _dummy2 : 8;
		
		uint16_t payload_len;					// Длина payload
		union
		{
			// Если пакет FORMAT_RAW
			structpack
			{
				uint8_t data[SECURITY_PAYLOAD_RAW_LEN];	// Полностью сырой payload
			} payload_raw;
			
			// Если пакет FORMAT_SIGN
			structpack
			{
				uint8_t iv[SECURITY_IV_LEN];	// Вектор инициализации
				uint8_t data[SECURITY_PAYLOAD_LEN];	// Данные открытые
				uint8_t tag[SECURITY_TAG_LEN];	// Тег аутентификации
			} payload_sign;
			
			// Если пакет FORMAT_ENCRYPT
			structpack
			{
				uint8_t iv[SECURITY_IV_LEN];	// Вектор инициализации
				uint8_t data[SECURITY_PAYLOAD_LEN];	// Данные зашифрованные
				uint8_t tag[SECURITY_TAG_LEN];	// Тег аутентификации
			} payload_enc;
		};
	};
};
