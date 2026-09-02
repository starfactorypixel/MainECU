#pragma once
#include <inttypes.h>
#include <string.h>
#include <CUtils_CRC16.h>
#include "PacketTransportBase.hpp"
#include "mbedtls/chachapoly.h"

class PacketTransportSignAll : public PacketTransportBase
{
	static constexpr format_t _format = FORMAT_SIGNALL;

	public:

		PacketTransportSignAll()
		{
			sizeof(transport_t);
		}

		void CfgTimeout(uint16_t timeout)
		{
			_param.timeout = timeout;
			return;
		}
		
		// Вставить payload в пакет
		bool PutPayload(const uint8_t *data, uint8_t length)
		{
			if(length > PAYLOAD_LEN) return false;
			
			memcpy(_packet.payload_sign.data, data, length);
			_packet.payload_len = IV_LEN + TAG_LEN + length;
			return true;
		}
		
		// Взять payload из пакета
		uint8_t GetPayload(uint8_t *&data)
		{
			if(_param.parsed == false) return 0;
			
			data = _packet.payload_sign.data;
			return _packet.payload_len - (IV_LEN + TAG_LEN);
		}
		
		// 
		void PutPacketByte(uint8_t data, uint32_t time)
		{
			if(time - _param.last_rx_time >= _param.timeout)
				_ReInit();
			
			_param.last_rx_time = time;
			
			if(_packet_put_idx >= PACKET_LEN_MAX) return _SetError(ERROR_OVERFLOW);
			
			((uint8_t *)&_packet)[_packet_put_idx++] = data;
			
			if(_packet_put_idx == PACKET_LEN_MIN)
			{
				if(_packet.payload_len > PAYLOAD_RAW_LEN) return _SetError(ERROR_PAYLOAD_LEN);
				
				_packet_length = PACKET_LEN_MIN + _packet.payload_len;
				return;
			}
			
			if(_packet_put_idx == _packet_length)
			{
				_Parse();
				return;
			}
		}

		// 
		bool GetPacketBytes(uint8_t *&data, uint8_t &length)
		{
			_Prepare();

			data = (uint8_t *)&_packet;
			length = _packet_length;
			return true;
		}









	
	private:
		

		void _ReInit()
		{
			_packet_length = 0;
			_packet_put_idx = 0;
			memset(&_packet, 0x00, sizeof(_packet));
			_param.parsed = false;
			_param.error = ERROR_NONE;
			

			return;
		}
		
		// Подготовка пакета перед отправкой
		void _Prepare()
		{
			_packet_length = PACKET_LEN_MIN + _packet.payload_len;
			_packet.head[0] = 'P'; _packet.head[1] = 'X'; _packet.head[2] = 'L';
			_packet.version = PACKET_VERSION;
			_packet.format = _format;
			_packet.direction = 0;
			_packet.crc = CRC_DEFAULT_VALUE;
			_Signing();
			_packet.crc = CRC16_XModem_Table((uint8_t *)&_packet, _packet_length);
			
			return;
		}
		
		// Разбор пакета при приёме
		void _Parse()
		{
			//if(_packet_length < PACKET_LEN_MIN || _packet_length > PACKET_LEN_MAX) return _SetError(ERROR_LEN);
			if(_packet.head[0] != 'P' || _packet.head[1] != 'X' || _packet.head[2] != 'L') return _SetError(ERROR_HEAD);
			if(_packet.version != PACKET_VERSION) return _SetError(ERROR_VERSION);
			if(_packet.format != _format) return _SetError(ERROR_FORMAT);
			if(_packet.direction != 1) return _SetError(ERROR_DIRECTION);
			//if(_packet.payload_len > PAYLOAD_LEN) return _SetError(ERROR_PAYLOAD_LEN);
			if(_CheckCRC() == false) return _SetError(ERROR_CRC);
			
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

		void _Signing()
		{
    mbedtls_chachapoly_context ctx;

    mbedtls_chachapoly_init(&ctx);

    if (mbedtls_chachapoly_setkey(&ctx, _tls_params.key) != 0)
    {
        mbedtls_chachapoly_free(&ctx);
        return /*false*/;
    }

    int ret = mbedtls_chachapoly_encrypt_and_tag(&ctx, 0, iv, data, data_len, nullptr, nullptr, tag);

    mbedtls_chachapoly_free(&ctx);

    return /*ret == 0*/;
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


		struct
		{
			uint8_t *iv;
			uint8_t *key;
		} _tls_params;

		
};
