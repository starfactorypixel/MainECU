#pragma once
#include <inttypes.h>
#include <string.h>
#include <CUtils_CRC16.h>

#define structpack		struct __attribute__((packed))







#include "mbedtls/chacha20.h"
#include "mbedtls/chachapoly.h"
#include "mbedtls/poly1305.h"

#define CHACHA20_KEY_LEN 32U
#define CHACHA20_IV_LEN 12U
#define POLY1305_TAG_LEN 16U

bool chacha20_poly1305_sign(const uint8_t *key, const uint8_t *iv, const uint8_t *data, size_t data_len, uint8_t *tag)
{
    mbedtls_chachapoly_context ctx;

    mbedtls_chachapoly_init(&ctx);

    if (mbedtls_chachapoly_setkey(&ctx, key) != 0)
    {
        mbedtls_chachapoly_free(&ctx);
        return false;
    }

    int ret = mbedtls_chachapoly_encrypt_and_tag(&ctx, 0, iv, data, data_len, nullptr, nullptr, tag);

    mbedtls_chachapoly_free(&ctx);

    return ret == 0;
}

bool chacha20_poly1305_verify(const uint8_t *key, const uint8_t *iv, const uint8_t *data, size_t data_len, const uint8_t *tag)
{
    mbedtls_chachapoly_context ctx;

    mbedtls_chachapoly_init(&ctx);

    if (mbedtls_chachapoly_setkey(&ctx, key) != 0)
    {
        mbedtls_chachapoly_free(&ctx);
        return false;
    }

    int ret = mbedtls_chachapoly_auth_decrypt(&ctx, 0, iv, data, data_len, tag, nullptr, nullptr);

    mbedtls_chachapoly_free(&ctx);

    return ret == 0;
}


bool chacha20_poly1305_encrypt(const uint8_t *key, const uint8_t *iv, const uint8_t *data, size_t data_len, uint8_t *encrypted, uint8_t *tag)
{
    mbedtls_chachapoly_context ctx;

    mbedtls_chachapoly_init(&ctx);

    if (mbedtls_chachapoly_setkey(&ctx, key) != 0)
    {
        mbedtls_chachapoly_free(&ctx);
        return false;
    }

    int ret = mbedtls_chachapoly_encrypt_and_tag(&ctx, data_len, iv, nullptr, 0, data, encrypted, tag);

    mbedtls_chachapoly_free(&ctx);

    return ret == 0;
}

bool chacha20_poly1305_decrypt(const uint8_t *key, const uint8_t *iv, uint8_t *data, size_t data_len, const uint8_t *encrypted, const uint8_t *tag)
{
    mbedtls_chachapoly_context ctx;

    mbedtls_chachapoly_init(&ctx);

    if (mbedtls_chachapoly_setkey(&ctx, key) != 0)
    {
        mbedtls_chachapoly_free(&ctx);
        return false;
    }

    int ret = mbedtls_chachapoly_auth_decrypt(&ctx, data_len, iv, nullptr, 0, tag, encrypted, data);

    mbedtls_chachapoly_free(&ctx);

    return ret == 0;
}




class L3PacketTransport
{
	static constexpr uint8_t IV_LEN = 12U;
	static constexpr uint8_t TAG_LEN = 16U;
	static constexpr uint8_t PAYLOAD_LEN = 64U;
	static constexpr uint8_t PAYLOAD_RAW_LEN = IV_LEN + TAG_LEN + PAYLOAD_LEN;
	static constexpr uint8_t PACKET_LEN_MIN = 5U;
	static constexpr uint8_t PACKET_LEN_MAX = PACKET_LEN_MIN + PAYLOAD_RAW_LEN;
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
		uint8_t _dummy1 : 5;
		uint8_t version : 3;					// Версия протокола: 0x02

		uint8_t _dummy2 : 5;
		format_t format : 2;					// Формат пакета
		uint8_t direction : 1;					// Флаг направления

		uint16_t crc;							// CRC16_XModem

		uint8_t length;							// Длина payload

		union
		{
			// Если пакет FORMAT_RAW
			structpack
			{
				uint8_t raw[PAYLOAD_RAW_LEN];
			} payload_0;

			// Если пакет FORMAT_SIGNALL
			structpack
			{
				uint8_t iv[IV_LEN];				// Вектор инициализации
				uint8_t tag[TAG_LEN];			// Тег аутентификации
				uint8_t data[PAYLOAD_LEN];		// Данные открытые
			} payload_1;

			// Если пакет FORMAT_ENCDATA_SIGNALL
			structpack
			{
				uint8_t iv[IV_LEN];				// Вектор инициализации
				uint8_t tag[TAG_LEN];			// Тег аутентификации
				uint8_t cipher[PAYLOAD_LEN];	// Данные зашифрованные
			} payload_3;
		}
	};

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
	};
	
	public:
		L3PacketTransport(uint8_t format) : _format((format_t)format)				// Убрать приведение типов, передавать сразу format_t
		{
			sizeof(transport_t);
		}

		// Вставить payload в пакет
		bool SetPayload(uint8_t *data, uint8_t length)
		{
			if(length > PAYLOAD_LEN) return false;
			
			memcpy(_packet.payload, data, length);
			_packet.length = length;
			return true;
		}

		// Взять payload из пакета
		uint8_t GetPayload(uint8_t *&data)
		{
			if(_param.parsed == false) return 0;

			data = _packet.payload;
			return _packet.length;
		}







		void PutPacketByte(uint8_t data, uint32_t time)
		{
			if(time - _param.last_rx_time >= _param.timeout)
				_ReInit();

			_param.last_rx_time = time;

			if(_packet_put_idx >= PACKET_LEN_MAX) return _SetError(ERROR_OVERFLOW);
			
			((uint8_t *)&_packet)[_packet_put_idx++] = data;

			if(_packet_put_idx == PACKET_LEN_MIN)
			{
				if(_packet.length > PAYLOAD_LEN) return _SetError(ERROR_PAYLOAD_LEN);
				
				_packet_length = PACKET_LEN_MIN + _packet.length + ((_packet.encryption) ? IV_LEN + TAG_LEN : 0);
				return;
			}
			
			if(_packet_put_idx == _packet_length)
			{
				_Parse();
				return;
			}
		}









	
	private:
		

		void _ReInit()
		{

		}
		
		// Подготовка пакета перед отправкой
		void _Prepare(bool enc)
		{
			_packet_length = PACKET_LEN_MIN + _packet.length;
			_packet.version = 0x02;
			_packet.direction = 0;
			if(enc == true)
				_Encrypt();
			_packet.crc = CRC_DEFAULT_VALUE;
			_packet.crc = CRC16_XModem_Table((uint8_t *)&_packet, _packet_length);

			return;
		}

		// Шифрование пакета перед отправкой
		void _Encrypt()
		{
			_packet_length += IV_LEN + TAG_LEN;
			_packet.encryption = 1;

			//шифрование
		}

		void _Decrypt()
		{
			_packet_length -= IV_LEN + TAG_LEN;
			_packet.encryption = 0;

			//дешифрование
		}


		// Разбор пакета при приёме
		void _Parse()
		{
			//if(_packet_length < PACKET_LEN_MIN || _packet_length > PACKET_LEN_MAX) return _SetError(ERROR_LEN);
			if(_packet.version != 0x02) return _SetError(ERROR_VERSION);
			if(_packet.direction != 1) return _SetError(ERROR_DIRECTION);
			//if(_packet.length > PAYLOAD_LEN) return _SetError(ERROR_PAYLOAD_LEN);
			if(_CheckCRC() == false) return _SetError(ERROR_CRC);

			// перенести в момент забора пакета, в рунтайм.
			if(_packet.encryption == 1)
				_Decrypt();
			
			_param.parsed = true;
			_param.error = ERROR_NONE;
			return;
		}

		bool _CheckCRC()
		{
			uint16_t old_crc = _packet.crc;
			_packet.crc = CRC_DEFAULT_VALUE;
			uint16_t new_crc = CRC16_XModem_Table((uint8_t *)&_packet, _packet_length);
			_packet.crc = old_crc;

			return (old_crc == new_crc);
		}

		void _SetError(error_t error)
		{
			_param.error = error;
			return;
		}

	
		transport_t _packet;			// Пакет
		uint8_t _packet_length;			// Фактическая длина пакета
		uint8_t _packet_put_idx;		// Смещение вставки пакета

		struct
		{
			bool parsed;
			error_t error;

			uint32_t last_rx_time;
			uint16_t timeout;
		} _param;

		format_t _format;
};
