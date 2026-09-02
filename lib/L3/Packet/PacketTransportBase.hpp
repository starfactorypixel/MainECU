#pragma once
#include <inttypes.h>

#define structpack		struct __attribute__((packed))

class PacketTransportBase
{
	public:
		
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
		};
		
	protected:
		
		static constexpr uint8_t IV_LEN = 12U;
		static constexpr uint8_t TAG_LEN = 16U;
		static constexpr uint8_t PAYLOAD_LEN = 64U;
		static constexpr uint8_t PAYLOAD_RAW_LEN = IV_LEN + TAG_LEN + PAYLOAD_LEN;
		static constexpr uint8_t PACKET_LEN_MIN = 5U;
		static constexpr uint8_t PACKET_LEN_MAX = PACKET_LEN_MIN + PAYLOAD_RAW_LEN;

		static constexpr uint8_t PACKET_VERSION = 0x02;
		static constexpr uint16_t CRC_DEFAULT_VALUE = 0x0000;
		
		enum format_t : uint8_t
		{
			FORMAT_RAW = 0,							// Данные открыты а весь payload доступен
			FORMAT_SIGNALL = 1,						// Данные открыты а весь пакет подписан
			FORMAT_RESERVED = 2,					// Зарезервировано
			FORMAT_ENCDATA_SIGNALL = 3,				// Данные зашифрованы а весь пакет подписан
		};

		structpack transport_t
		{
			uint8_t head[3];						// Заголовок

			uint8_t _dummy1 : 5;
			uint8_t version : 3;					// Версия протокола: 0x02

			uint8_t _dummy2 : 5;
			format_t format : 2;					// Формат пакета
			uint8_t direction : 1;					// Флаг направления

			uint16_t crc;							// CRC16_XModem

			uint8_t payload_len;					// Длина payload

			union
			{
				// Если пакет FORMAT_RAW
				structpack
				{
					uint8_t data[PAYLOAD_RAW_LEN];	// Полностью сырой payload
				} payload_raw;

				// Если пакет FORMAT_SIGNALL
				structpack
				{
					uint8_t iv[IV_LEN];				// Вектор инициализации
					uint8_t tag[TAG_LEN];			// Тег аутентификации
					uint8_t data[PAYLOAD_LEN];		// Данные открытые
				} payload_sign;

				// Если пакет FORMAT_ENCDATA_SIGNALL
				structpack
				{
					uint8_t iv[IV_LEN];				// Вектор инициализации
					uint8_t tag[TAG_LEN];			// Тег аутентификации
					uint8_t data[PAYLOAD_LEN];		// Данные зашифрованные
				} payload_enc;
			};
		};
};
