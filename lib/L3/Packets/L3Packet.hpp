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

		// format — 3-битное поле (0..7), таблица на все 8 значений, выход за неё невозможен.
		// capacity == 0 — формат зарезервирован. Новый формат с иной раскладкой payload потребует
		// своей ветки в _AADLen()/_CryptLen()/_Gcm(), остальное берётся из этой таблицы.
		// Таблица локальная: нет определения вне класса, безопасно для нескольких единиц трансляции
		const format_info_t &_Info() const
		{
			static constexpr format_info_t table[L3C::FORMAT_COUNT] =
			{
				{0,  0,                                                 L3C::PACKET_PAYLOAD_RAW_LEN},	// 0 RAW
				{L3C::PACKET_IV_LEN, L3C::PACKET_IV_LEN + L3C::PACKET_TAG_LEN, L3C::PACKET_DATA_LEN_MAX},	// 1 SIGN
				{0,  0,                                                 0},								// 2 резерв
				{L3C::PACKET_IV_LEN, L3C::PACKET_IV_LEN + L3C::PACKET_TAG_LEN, L3C::PACKET_DATA_LEN_MAX},	// 3 ENCRYPT
				{0,  0,                                                 0},								// 4 резерв
				{0,  0,                                                 0},								// 5 резерв
				{0,  0,                                                 0},								// 6 резерв
				{0,  0,                                                 0},								// 7 резерв
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

		L3C::IV_t *_IV()
		{
			return (L3C::IV_t *)_packet.payload;
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

		// Подпись/шифрование (encrypt) или проверка/расшифровка (!encrypt) на месте.
		// bind — PACKET_AAD_BIND_LEN байт, дописываемых в конец AAD (идентификатор сеанса для пакетов клиента), или nullptr.
		// Контекст GCM готовится один раз в _CfgGcm() (из CfgKey()): расписание ключа AES и H не пересчитываются
		// на каждый пакет. Повторное использование контекста допустимо: каждый вызов mbedtls
		// начинает новое сообщение со своим IV. Rx и Tx — разные объекты, контексты не делятся между задачами.
		// Возвращает код mbedtls, 0 — успех
		int _Gcm(bool encrypt, const uint8_t *bind = nullptr)
		{
			if(_gcm_ready == false) return MBEDTLS_ERR_GCM_BAD_INPUT;

			uint8_t *data = _PayloadData();
			uint16_t crypt_len = _CryptLen();

			// AAD одним куском: в пакете за DATA лежит тег, дописать на месте нельзя — собираем копию
			const uint8_t *aad = _AAD();
			uint16_t aad_len = _AADLen();
			uint8_t aad_buf[L3C::PACKET_HEADER_LEN + L3C::PACKET_IV_LEN + L3C::PACKET_DATA_LEN_MAX + L3C::PACKET_AAD_BIND_LEN];
			if(bind != nullptr)
			{
				memcpy(aad_buf, aad, aad_len);
				memcpy(aad_buf + aad_len, bind, L3C::PACKET_AAD_BIND_LEN);
				aad = aad_buf;
				aad_len += L3C::PACKET_AAD_BIND_LEN;
			}

			if(encrypt)
				return mbedtls_gcm_crypt_and_tag(&_gcm, MBEDTLS_GCM_ENCRYPT, crypt_len,
					_IV()->raw, L3C::PACKET_IV_LEN, aad, aad_len,
					data, data, L3C::PACKET_TAG_LEN, _Tag());

			return mbedtls_gcm_auth_decrypt(&_gcm, crypt_len,
				_IV()->raw, L3C::PACKET_IV_LEN, aad, aad_len,
				_Tag(), L3C::PACKET_TAG_LEN, data, data);
		}

		// Только GCM: расписание ключа AES и H считаются здесь один раз, а не на каждый пакет
		void _CfgGcm()
		{
			mbedtls_gcm_free(&_gcm);
			mbedtls_gcm_init(&_gcm);
			_gcm_ready = (_param.key != nullptr) && (mbedtls_gcm_setkey(&_gcm, MBEDTLS_CIPHER_ID_AES, _param.key, 256) == 0);

			return;
		}

		bool _SetError(L3C::error_t error)
		{
			_param.error = error;
			return false;
		}

		L3C::packet_t _packet;
		L3PacketSeq *_seq;
		mbedtls_gcm_context _gcm;
		bool _gcm_ready;

		struct
		{
			const uint8_t *key;
			uint8_t type;			// ESP: instance в IV, ожидаемый instance клиента — его инверсия
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

			mbedtls_gcm_init(&_gcm);
			_gcm_ready = false;
		}

		~L3Packet()
		{
			mbedtls_gcm_free(&_gcm);
		}

		L3Packet(const L3Packet &) = delete;
		L3Packet &operator=(const L3Packet &) = delete;

		// Параметры защиты канала, от шифра не зависят: key — 32 байта (указатель в Config, хранится),
		// type — instance в IV, seq — счётчики и идентификатор сеанса. Затем готовятся шифры всех форматов.
		// Шифры копируют ключ в свои контексты: если ключ в Config поменялся (новое сопряжение), CfgKey()
		// нужно вызвать снова. Вызывать при остановленном приёме: задача приёма работает с теми же контекстами
		void CfgKey(const uint8_t key[32], uint8_t type, L3PacketSeq *seq)
		{
			_param.key = key;
			_param.type = type;
			_seq = seq;

			_CfgGcm();		// SIGN, ENCRYPT. Новый формат со своим шифром — свой _CfgXxx() здесь

			return;
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
