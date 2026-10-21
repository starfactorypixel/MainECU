#pragma once
#include <inttypes.h>
#include <string.h>
#include <CUtils_CRC16.h>
#include "L3PacketConsts.hpp"
#include "L3PacketSeq.hpp"
#include "mbedtls/gcm.h"

namespace L3C = L3PacketConsts;

// Общая часть Rx/Tx: буфер пакета, разметка, CRC, GCM. Состояние только общее для обеих сторон.
class L3Packet
{
	protected:

		static_assert(sizeof(L3C::packet_t) == L3C::PACKET_LEN_MAX,
			"L3C::packet_t size mismatch with PACKET_LEN_MAX");

		struct format_info_t
		{
			uint8_t data_offset;
			uint8_t overhead;
			uint8_t capacity;
		};

		// format — 2-битное поле (0..3), выход за таблицу невозможен.
		// Таблица локальная: нет определения вне класса, безопасно для нескольких единиц трансляции
		const format_info_t &_Info() const
		{
			static constexpr format_info_t table[4] =
			{
				{0,  0,                                                 L3C::PACKET_PAYLOAD_RAW_LEN},	// RAW
				{L3C::PACKET_IV_LEN, L3C::PACKET_IV_LEN + L3C::PACKET_TAG_LEN, L3C::PACKET_DATA_LEN_MAX},	// SIGN
				{0,  0,                                                 0},								// RESERVED
				{L3C::PACKET_IV_LEN, L3C::PACKET_IV_LEN + L3C::PACKET_TAG_LEN, L3C::PACKET_DATA_LEN_MAX},	// ENCRYPT
			};

			return table[_packet.format];
		}

		uint16_t _PayloadLength() const
		{
			const format_info_t &f = _Info();

			if(_packet.payload_len < f.overhead)
				return 0;

			return _packet.payload_len - f.overhead;
		}

		uint8_t *_PayloadData()
		{
			return &_packet.payload[_Info().data_offset];
		}

		uint8_t *_IV()
		{
			return _packet.payload;
		}

		uint8_t *_Tag()
		{
			return &_packet.payload[_Info().data_offset + _PayloadLength()];
		}

		// Длина пакета без CRC
		uint16_t _PacketLength() const
		{
			return L3C::PACKET_HEADER_LEN + _packet.payload_len;
		}

		// CRC лежит сразу за фактическими данными, а не по фиксированному смещению в структуре
		uint8_t *_CRCPtr()
		{
			return (uint8_t *)&_packet + _PacketLength();
		}

		uint16_t _ReadCRC()
		{
			const uint8_t *p = _CRCPtr();

			return (uint16_t)(p[0] | (p[1] << 8));
		}

		void _WriteCRC(uint16_t value)
		{
			uint8_t *p = _CRCPtr();

			p[0] = (uint8_t)value;
			p[1] = (uint8_t)(value >> 8);

			return;
		}

		uint16_t _ComputeCRC() const
		{
			return CRC16_XModem_Table((uint8_t *)&_packet, _PacketLength());
		}

		// AAD начинается с байта 0 пакета. SIGN: заголовок+IV+данные, ENCRYPT: заголовок+IV
		uint8_t *_AAD()
		{
			return (uint8_t *)&_packet;
		}

		uint16_t _AADLen() const
		{
			uint16_t len = L3C::PACKET_HEADER_LEN + L3C::PACKET_IV_LEN;

			if(_packet.format == L3C::FORMAT_SIGN)
				len += _PayloadLength();

			return len;
		}

		// Сколько байт шифруется (SIGN — ничего, только подпись)
		uint16_t _CryptLen() const
		{
			return (_packet.format == L3C::FORMAT_ENCRYPT) ? _PayloadLength() : 0;
		}

		// Номер пакета N из IV (раскладка в L3PacketConsts.hpp)
		uint64_t _IVGetN()
		{
			const uint8_t *iv = _IV();

			uint32_t epoch = (uint32_t)iv[0] | ((uint32_t)iv[1] << 8) | ((uint32_t)iv[2] << 16);
			uint32_t counter = (uint32_t)iv[4] | ((uint32_t)iv[5] << 8) | ((uint32_t)iv[6] << 16) | ((uint32_t)iv[7] << 24);

			return ((uint64_t)epoch << 32) | counter;
		}

		// Подпись/шифрование (encrypt) или проверка/расшифровка (!encrypt) на месте.
		// Возвращает код mbedtls, 0 — успех
		int _Gcm(bool encrypt)
		{
			mbedtls_gcm_context ctx;
			mbedtls_gcm_init(&ctx);

			int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, _param.key, 256);
			if(ret == 0)
			{
				uint8_t *data = _PayloadData();
				uint16_t crypt_len = _CryptLen();

				if(encrypt)
					ret = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, crypt_len,
						_IV(), L3C::PACKET_IV_LEN, _AAD(), _AADLen(),
						data, data, L3C::PACKET_TAG_LEN, _Tag());
				else
					ret = mbedtls_gcm_auth_decrypt(&ctx, crypt_len,
						_IV(), L3C::PACKET_IV_LEN, _AAD(), _AADLen(),
						_Tag(), L3C::PACKET_TAG_LEN, data, data);
			}

			mbedtls_gcm_free(&ctx);

			return ret;
		}

		bool _SetError(L3C::error_t error)
		{
			_param.error = error;
			return false;
		}

		L3C::packet_t _packet;
		L3PacketSeq *_seq;

		struct
		{
			const uint8_t *key;
			uint8_t type;			// ESP: instance в IV, ожидаемый instance партнёра — его инверсия
			L3C::error_t error;
		} _param;

	public:

		L3Packet()
		{
			memset(&_packet, 0x00, sizeof(_packet));

			_seq = nullptr;
			_param.key = nullptr;
			_param.type = 0;
			_param.error = L3C::ERROR_NONE;
		}

		// key — 32 байта, должен жить не меньше объекта (ссылка на Config, не на копию)
		void CfgKey(const uint8_t key[32], uint8_t type, L3PacketSeq *seq)
		{
			_param.key = key;
			_param.type = type;
			_seq = seq;
		}

		bool IsError()
		{
			return _param.error != L3C::ERROR_NONE;
		}

		L3C::error_t GetError()
		{
			return _param.error;
		}
};
