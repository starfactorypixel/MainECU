#pragma once
#include <inttypes.h>
#include <string.h>
#include "L3PacketConsts.hpp"
#include "mbedtls/chachapoly.h"
#include "esp_random.h"

namespace L3 = L3PacketConsts;

class L3PacketSecurityEncrypt
{
	static constexpr L3::format_t _format = L3::FORMAT_ENCRYPT;
	
	public:

		L3PacketSecurityEncrypt()
		{
			sizeof(L3::security_t);
		}
		
		void Start()
		{
			esp_fill_random(_packet.payload_enc.iv, L3::SECURITY_IV_LEN);
			_packet.payload_enc.iv[7] ^= _param.type;

			return;
		}
		
		void CfgKey(uint8_t key[32], uint8_t type)
		{
			_param.key = key;
			_param.type = type;
			
			return;
		}
		
		// Вставить пакет целиком при приёме
		bool PutPacketPtr(const uint8_t *data, uint16_t length)
		{
			if(length < (L3::SECURITY_PACKET_LEN_MIN + L3::SECURITY_IV_LEN + L3::SECURITY_TAG_LEN) || length > L3::SECURITY_PACKET_LEN_MAX) return false;
			
			memcpy(&_packet, data, length);
			_packet_length = length;

			_Parse();
			return true;
		}
		
		// Взять весь пакет целиком для отправки
		// При вызове пакет автоматически подготовится
		uint16_t GetPacketPtr(uint8_t *&data)
		{
			_Prepare();
			
			data = (uint8_t *)&_packet;
			return _packet_length;
		}
		
		// Вставить payload в пакет
		// Вставляет в начало, переписывая содержимое
		bool PutPayload(const uint8_t *data, uint16_t length)
		{
			if(length > L3::SECURITY_PAYLOAD_LEN) return false;
			
			memcpy(_packet.payload_enc.data, data, length);
			_packet.payload_len = length;
			return true;
		}
		
		// Добавить payload в пакет
		// Добавляет к уже добавленным данным
		bool AddPayload(const uint8_t *data, uint16_t length)
		{
			if(length > L3::SECURITY_PAYLOAD_LEN) return false;
			if(_packet.payload_len + length > L3::SECURITY_PAYLOAD_LEN) return false;
			
			memcpy(&_packet.payload_enc.data[_packet.payload_len], data, length);
			_packet.payload_len += length;
			return true;
		}
		
		// Взять payload из пакета для анализа
		// Если пакет не готов, то вернёт 0
		uint16_t GetPayloadPtr(uint8_t *&data)
		{
			if(_param.parsed == false) return 0;
			
			data = _packet.payload_enc.data;
			return _packet.payload_len;
		}

		// Проверить факт разбора входящего пакета
		bool IsParsed()
		{
			return _param.parsed;
		}

		// Проверить наличие ошибок
		bool IsError()
		{
			return (_param.error != L3::ERROR_NONE);
		}

		// Получить код ошибки
		uint8_t GetError()
		{
			return _param.error;
		}
		
		// Сбросить объект в начальное состояние
		void Init()
		{
			_packet_length = 0;
			memset(&_packet, 0x00, L3::SECURITY_PACKET_LEN_MIN);
			_param.error = L3::ERROR_NONE;
			_param.parsed = false;
			
			return;
		}
		
	private:
		
		// Подготовка пакета перед отправкой
		void _Prepare()
		{
			uint16_t payload_len = _packet.payload_len;

			_packet_length = L3::SECURITY_PACKET_LEN_MIN + L3::SECURITY_IV_LEN + L3::SECURITY_TAG_LEN + _packet.payload_len;
			_packet.format = _format;
			_packet.payload_len += L3::SECURITY_IV_LEN + L3::SECURITY_TAG_LEN;
			_Encrypting();

			// _Signing() пишет tag в своё поле структуры. Нам же нужно всё сжать в один массив, поэтому переносим tag сразу после конца payload
			memmove(_packet.payload_enc.data + payload_len, _packet.payload_enc.tag, L3::SECURITY_TAG_LEN);
			// Возможно тут проблема, поскольку мы можем читать хвост tag уже за пределами data.
			
			return;
		}
		
		// Разбор пакета при приёме
		void _Parse()
		{
			if(_packet.format != _format) return _SetError(L3::ERROR_FORMAT);
			if(_packet.payload_len < (L3::SECURITY_IV_LEN + L3::SECURITY_TAG_LEN)) return _SetError(L3::ERROR_MIN_LEN);
			
			_packet.payload_len -= (L3::SECURITY_IV_LEN + L3::SECURITY_TAG_LEN);
			
			if(_packet.payload_len > L3::SECURITY_PAYLOAD_LEN) return _SetError(L3::ERROR_LEN);

			// Пакет приходит сжатый, т.е. tag сразу после payload, поэтому мы переносим tag в нужное место структуры
			memmove(_packet.payload_enc.tag, _packet.payload_enc.data + _packet.payload_len, L3::SECURITY_TAG_LEN);
			// Возможно тут проблема, поскольку мы можем читать хвост tag уже за пределами data.
			
			if(_Decrypting() == true)
			{
				_param.error = L3::ERROR_NONE;
				_param.parsed = true;
			}
			return;
		}
		
		void _SetError(L3::error_t error)
		{
			_param.error = error;
			return;
		}
		
		bool _Encrypting()
		{
			mbedtls_chachapoly_context ctx;
			
			mbedtls_chachapoly_init(&ctx);
			if(mbedtls_chachapoly_setkey(&ctx, _param.key) != 0)
			{
				mbedtls_chachapoly_free(&ctx);
				return false;
			}

			uint8_t *aadata = (uint8_t *)&_packet;
			uint16_t aadata_len = L3::SECURITY_PACKET_LEN_MIN + L3::SECURITY_IV_LEN;
			uint16_t data_len = _packet.payload_len - (L3::SECURITY_IV_LEN + L3::SECURITY_TAG_LEN);
			(*(uint32_t *)&_packet.payload_enc.iv[8])++;
			int ret = mbedtls_chachapoly_encrypt_and_tag(&ctx, data_len, _packet.payload_enc.iv, aadata, aadata_len, _packet.payload_enc.data, _packet.payload_enc.data, _packet.payload_enc.tag);
			
			mbedtls_chachapoly_free(&ctx);
			
			return (ret == 0);
		}
		
		bool _Decrypting()
		{
			mbedtls_chachapoly_context ctx;
			
			mbedtls_chachapoly_init(&ctx);
			if(mbedtls_chachapoly_setkey(&ctx, _param.key) != 0)
			{
				mbedtls_chachapoly_free(&ctx);
				return false;
			}
			
			uint8_t *aadata = (uint8_t *)&_packet;
			uint16_t aadata_len = L3::SECURITY_PACKET_LEN_MIN + L3::SECURITY_IV_LEN;
			uint16_t data_len = _packet.payload_len - (L3::SECURITY_IV_LEN + L3::SECURITY_TAG_LEN);
			int ret = mbedtls_chachapoly_auth_decrypt(&ctx, data_len, _packet.payload_enc.iv, aadata, aadata_len, _packet.payload_enc.tag, _packet.payload_enc.data, _packet.payload_enc.data);
			if(ret == MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED) _SetError(L3::ERROR_AUTH_FAILED);
			
			mbedtls_chachapoly_free(&ctx);
			
			return (ret == 0);
		}
		
		L3::security_t _packet;			// Пакет
		uint16_t _packet_length;		// Фактическая длина пакета
		
		struct
		{
			uint32_t last_rx_time;
			uint16_t timeout;
			uint8_t *key;
			uint8_t type;
			L3::error_t error;
			bool parsed;
		} _param;
};
