#pragma once
#include <inttypes.h>
#include <string.h>
#include "L3PacketConsts.hpp"
#include "mbedtls/chachapoly.h"
#include "esp_random.h"

/*
3. Нет проверки _param.key == nullptr перед крипто-операциями
Даже после исправления п.2, если CfgKey() не вызвали, _param.key == nullptr уйдёт в mbedtls и вызовет чтение 32 байт по нулевому указателю. В _Signing()/_Verifying()/_Encrypting()/_Decrypting() стоит добавить проверку до setkey:

if(_param.key == nullptr)
    return false;




GetPacketPtr() не возвращает никакого признака ошибки — стоит, как минимум, документировать, что перед вызовом нужно проверять IsError(), либо возвращать 0 при неудачной подготовке (см. п.1).



Исправление — запрещать Add/PutPayload после того, как пакет уже был подготовлен один раз для ENCRYPT/SIGN, либо явно требовать Init()/Start() перед новым раундом:
bool AddPayload(const uint8_t *data, uint16_t length)
{
    if(_param.prepared)          // пакет уже подписан/зашифрован — дозапись запрещена
    {
        _SetError(L3C::ERROR_FORMAT);
        return false;
    }
    ...

(аналогично для PutPayload, если формат не RAW и _param.prepared==true, — либо документировать, что после GetPacketPtr() объект нужно инициализировать заново перед следующей отправкой).




*/

namespace L3C = L3PacketConsts;




#include <CUtils.h>





structpack unaligned_u32_t
{
	uint32_t value;
};







class L3PacketSecurity
{
	protected:

		struct format_info_t
		{
			uint8_t data_offset;
			uint8_t overhead;
			uint8_t capacity;
			L3C::format_t format;
		};

		static constexpr format_info_t _format_info[] =
		{
			{0,   0,   L3C::SECURITY_PAYLOAD_RAW_LEN, L3C::FORMAT_RAW},
			{12,  28,  L3C::SECURITY_PAYLOAD_LEN,     L3C::FORMAT_SIGN},
			{0,   0,   0,                             L3C::FORMAT_RAW},
			{12,  28,  L3C::SECURITY_PAYLOAD_LEN,     L3C::FORMAT_ENCRYPT}
		};

		const format_info_t &_GetFormat() const
		{
			static constexpr format_info_t invalid = {};
			
			uint8_t format = (uint8_t)_packet.format;
			if(format >= sizeofarray(_format_info))
				return invalid;
				
			return _format_info[format];
		}
		
		uint16_t _PayloadLength() const
		{
			const format_info_t &format = _GetFormat();
			
			if(_packet.payload_len < format.overhead)
				return 0;
				
			return _packet.payload_len - format.overhead;
		}
		
		uint8_t *_PayloadData()
		{
			return &_packet.payload[_GetFormat().data_offset];
		}
		
		uint8_t *_IV()
		{
			return _packet.payload;
		}
		
		uint8_t *_Tag()
		{
			return &_packet.payload[_GetFormat().data_offset + _PayloadLength()];
		}
		
		inline void _IVInc()
		{
			reinterpret_cast<unaligned_u32_t *>(_IV())->value++;

			return;
		}
		
		bool _SetError(L3C::error_t error)
		{
			_param.error = error;
			
			return false;
		}
		
		bool _Signing()
		{
			mbedtls_chachapoly_context ctx;
			
			mbedtls_chachapoly_init(&ctx);
			if(mbedtls_chachapoly_setkey(&ctx, _param.key) != 0)
			{
				mbedtls_chachapoly_free(&ctx);
				return false;
			}
			
			uint8_t *aadata = (uint8_t *)&_packet;
			uint16_t aadata_len = L3C::SECURITY_PACKET_LEN_MIN + L3C::SECURITY_IV_LEN + _PayloadLength();
			
			_IVInc();
			int ret = mbedtls_chachapoly_encrypt_and_tag(&ctx, 0, _IV(), aadata, aadata_len, nullptr, nullptr, _Tag());
			mbedtls_chachapoly_free(&ctx);
			
			return (ret == 0);
		}
		
		bool _Verifying()
		{
			mbedtls_chachapoly_context ctx;
			
			mbedtls_chachapoly_init(&ctx);
			if(mbedtls_chachapoly_setkey(&ctx, _param.key) != 0)
			{
				mbedtls_chachapoly_free(&ctx);
				return false;
			}
			
			uint8_t *aadata = (uint8_t *)&_packet;
			uint16_t aadata_len = L3C::SECURITY_PACKET_LEN_MIN + L3C::SECURITY_IV_LEN + _PayloadLength();
			
			int ret = mbedtls_chachapoly_auth_decrypt(&ctx, 0, _IV(), aadata, aadata_len, _Tag(), nullptr, nullptr);
			if(ret == MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED)
				_SetError(L3C::ERROR_AUTH_FAILED);
			mbedtls_chachapoly_free(&ctx);
			
			return (ret == 0);
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
			uint16_t aadata_len = L3C::SECURITY_PACKET_LEN_MIN + L3C::SECURITY_IV_LEN;
			uint16_t data_len = _PayloadLength();

			_IVInc();
			int ret = mbedtls_chachapoly_encrypt_and_tag(&ctx, data_len, _IV(), aadata, aadata_len, _PayloadData(), _PayloadData(), _Tag());
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
			uint16_t aadata_len = L3C::SECURITY_PACKET_LEN_MIN + L3C::SECURITY_IV_LEN;
			uint16_t data_len = _PayloadLength();
			
			int ret = mbedtls_chachapoly_auth_decrypt(&ctx, data_len, _IV(), aadata, aadata_len, _Tag(), _PayloadData(), _PayloadData());
			if(ret == MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED)
				_SetError(L3C::ERROR_AUTH_FAILED);
			mbedtls_chachapoly_free(&ctx);
			
			return (ret == 0);
		}
		
		L3C::security_t _packet;
		
		struct
		{
			const uint8_t *key;
			uint8_t type;
			L3C::error_t error;
		} _param;
		
	public:
		
		void CfgKey(const uint8_t key[32], uint8_t type)
		{
			_param.key = key;
			_param.type = type;
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

class L3PacketSecurityRx final : private L3PacketSecurity
{
	public:

		using L3PacketSecurity::CfgKey;
		using L3PacketSecurity::IsError;
		using L3PacketSecurity::GetError;

		void Init(L3C::format_t format = L3C::FORMAT_RAW)
		{
			memset(&_packet, 0x00, L3C::SECURITY_PACKET_LEN_MIN);
			
			_packet.format = format;
			_packet.payload_len = _GetFormat().overhead;
			
			_param.error = L3C::ERROR_NONE;
			_parsed = false;
			
			return;
		}
		bool PutPacketPtr(const uint8_t *data, uint16_t length)
		{
			if(length < L3C::SECURITY_PACKET_LEN_MIN || length > L3C::SECURITY_PACKET_LEN_MAX)
				return false;
				
			memcpy(&_packet, data, length);
			
			if(_packet.payload_len > L3C::SECURITY_PAYLOAD_RAW_LEN)
				return _SetError(L3C::ERROR_LEN);
			
			if(L3C::SECURITY_PACKET_LEN_MIN + _packet.payload_len != length)
				return _SetError(L3C::ERROR_LEN);
			
			_Parse();
			return true;
		}
		
		uint16_t GetPayloadPtr(uint8_t *&data)
		{
			if(_parsed == false)
				return 0;
			
			data = _PayloadData();
			return _PayloadLength();
		}
		
		bool IsParsed()
		{
			return _parsed;
		}
		
	private:
		
		bool _Parse()
		{
			const format_info_t &format = _GetFormat();
			
			_parsed = false;				////
			
			if(format.capacity == 0)
				return _SetError(L3C::ERROR_FORMAT);
			
			if(_packet.payload_len < format.overhead)
				return _SetError(L3C::ERROR_MIN_LEN);
			
			if(_PayloadLength() > format.capacity)
				return _SetError(L3C::ERROR_LEN);
			
			switch(format.format)
			{
				case L3C::FORMAT_RAW:
				{
					_param.error = L3C::ERROR_NONE;
					_parsed = true;
					
					break;
				}
				
				case L3C::FORMAT_SIGN:
				{
					if(_Verifying())
					{
						_param.error = L3C::ERROR_NONE;
						_parsed = true;
					}
					
					break;
				}
				
				case L3C::FORMAT_ENCRYPT:
				{
					if(_Decrypting())
					{
						_param.error = L3C::ERROR_NONE;
						_parsed = true;
					}
					
					break;
				}
				
				default:
				{
					_SetError(L3C::ERROR_FORMAT);
					
					break;
				}
			}
			
			return;
		}
		
		bool _parsed = false;
};

class L3PacketSecurityTx final : private L3PacketSecurity
{
	public:

		using L3PacketSecurity::CfgKey;
		using L3PacketSecurity::IsError;
		using L3PacketSecurity::GetError;
		
		void Init(L3C::format_t format = L3C::FORMAT_RAW)
		{
			memset(&_packet, 0x00, L3C::SECURITY_PACKET_LEN_MIN);
			
			_packet.format = format;
			_packet.payload_len = _GetFormat().overhead;
			
			_param.error = L3C::ERROR_NONE;
			_prepared = false;
			
			return;
		}
		
		void Start()
		{
			const format_info_t &format = _GetFormat();
			
			if(format.format == L3C::FORMAT_RAW || format.overhead == 0)
				return;
				
			esp_fill_random(_IV(), L3C::SECURITY_IV_LEN);
			_IV()[4] ^= _param.type;
			_prepared = false;
			
			return;
		}
		
		bool PutPayload(const uint8_t *data, uint16_t length)
		{
			const format_info_t &format = _GetFormat();
			
			if(format.capacity == 0)
				return _SetError(L3C::ERROR_FORMAT);
			
			if(length > format.capacity)
				return _SetError(L3C::ERROR_OVERFLOW);
			
			if(length)
				memcpy(&_packet.payload[format.data_offset], data, length);		// или _PayloadData()
			
			_packet.payload_len = format.overhead + length;
			_param.error = L3C::ERROR_NONE;
			_prepared = false;
			
			return true;
		}
		
		bool AddPayload(const uint8_t *data, uint16_t length)
		{
			const format_info_t &format = _GetFormat();
			uint16_t payload_length = _PayloadLength();
			
			if(format.capacity == 0)
				return _SetError(L3C::ERROR_FORMAT);
			
			if(payload_length > format.capacity)
				return _SetError(L3C::ERROR_LEN);
			
			if(length > format.capacity - payload_length)
				return _SetError(L3C::ERROR_OVERFLOW);
			
			if(length)
				memcpy(&_packet.payload[format.data_offset + payload_length], data, length);
			
			_packet.payload_len = format.overhead + payload_length + length;
			_param.error = L3C::ERROR_NONE;
			_prepared = false;
			
			return true;
		}
		
		uint16_t GetPacketPtr(uint8_t *&data)
		{
			_Prepare();
			
			if(_prepared == false)
				return 0;
			
			data = (uint8_t *)&_packet;
			return L3C::SECURITY_PACKET_LEN_MIN + _packet.payload_len;
		}
		
	private:
		
		bool _Prepare()												// настроить тип
		{
			if(_prepared)
				return;
			
			const format_info_t &format = _GetFormat();
			switch(format.format)
			{
				case L3C::FORMAT_RAW:
				{
					_prepared = true;
					
					break;
				}
				
				case L3C::FORMAT_SIGN:
				{
					if(_Signing())
						_prepared = true;
					else
						_SetError(L3C::ERROR_SIGN);
					
					break;
				}
				
				case L3C::FORMAT_ENCRYPT:
				{
					if(_Encrypting())
						_prepared = true;
					else
						_SetError(L3C::ERROR_ENCRYPT);
					
					break;
				}
				
				default:
				{
					_SetError(L3C::ERROR_FORMAT);
					
					break;
				}
			}
			
			return;
		}
		
		bool _prepared = false;				// Убрать, поскольку если не парсед то есть ошибка (error != ERROR_NONE)
};



























class L3PacketSecurity777
{
	struct format_info_t
	{
		uint8_t data_offset;
		uint8_t overhead;
		uint8_t capacity;
		L3C::format_t format;
	};


	static constexpr format_info_t _format_info[] = 
	{
		{0,   0,   L3C::SECURITY_PAYLOAD_RAW_LEN, L3C::FORMAT_RAW},
		{12,  28,  L3C::SECURITY_PAYLOAD_LEN,     L3C::FORMAT_SIGN},
		{0,   0,   0,                             L3C::FORMAT_RAW},
		{12,  28,  L3C::SECURITY_PAYLOAD_LEN,     L3C::FORMAT_ENCRYPT}
	};

	public:

		L3PacketSecurity777() : _param{}
		{
		}
		
		void Start()
		{
			const format_info_t &format = _GetFormat();

			if(format.format == L3C::FORMAT_RAW || format.overhead == 0)
				return;

			esp_fill_random(_packet.payload, L3C::SECURITY_IV_LEN);
			_packet.payload[7] ^= _param.type;
			_param.prepared = false;

			return;
		}
		
		void CfgKey(uint8_t key[32], uint8_t type)
		{
			_param.key = key;
			_param.type = type;
			
			return;
		}
		
		bool PutPacketPtr(const uint8_t *data, uint16_t length)
		{
			if(length < L3C::SECURITY_PACKET_LEN_MIN || length > L3C::SECURITY_PACKET_LEN_MAX)
				return false;
			
			memcpy(&_packet, data, length);

			if(_packet.payload_len > L3C::SECURITY_PAYLOAD_RAW_LEN)
			{
				_SetError(L3C::ERROR_LEN);
				return false;
			}

			if(L3C::SECURITY_PACKET_LEN_MIN + _packet.payload_len != length)
			{
				_SetError(L3C::ERROR_LEN);
				return false;
			}

			_param.prepared = false;
			_Parse();

			return true;
		}
		
		uint16_t GetPacketPtr(uint8_t *&data)
		{
			_Prepare();

			if(_param.prepared == false) return 0;
			
			data = (uint8_t *)&_packet;
			return L3C::SECURITY_PACKET_LEN_MIN + _packet.payload_len;
		}
		
		bool PutPayload(const uint8_t *data, uint16_t length)
		{
			const format_info_t &format = _GetFormat();

			if(format.capacity == 0)
			{
				_SetError(L3C::ERROR_FORMAT);
				return false;
			}

			if(length > format.capacity)
			{
				_SetError(L3C::ERROR_OVERFLOW);
				return false;
			}
			
			if(length)
				memcpy(&_packet.payload[format.data_offset], data, length);

			_packet.payload_len = format.overhead + length;
			_param.parsed = false;
			_param.error = L3C::ERROR_NONE;
			_param.prepared = false;

			return true;
		}

		bool AddPayload(const uint8_t *data, uint16_t length)
		{
			const format_info_t &format = _GetFormat();
			uint16_t payload_length = _PayloadLength();

			if(format.capacity == 0)
			{
				_SetError(L3C::ERROR_FORMAT);
				return false;
			}

			if(payload_length > format.capacity)
			{
				_SetError(L3C::ERROR_LEN);
				return false;
			}

			if(length > format.capacity - payload_length)
			{
				_SetError(L3C::ERROR_OVERFLOW);
				return false;
			}
			
			if(length)
				memcpy(&_packet.payload[format.data_offset + payload_length], data, length);

			_packet.payload_len = format.overhead + payload_length + length;
			_param.parsed = false;
			_param.error = L3C::ERROR_NONE;
			_param.prepared = false;

			return true;
		}
		
		uint16_t GetPayloadPtr(uint8_t *&data)
		{
			if(_param.parsed == false)
				return 0;

			data = &_packet.payload[_GetFormat().data_offset];
			return _PayloadLength();
		}

		bool IsParsed()
		{
			return _param.parsed;
		}

		bool IsError()
		{
			return (_param.error != L3C::ERROR_NONE);
		}

		L3C::error_t GetError()
		{
			return _param.error;
		}
		
		void Init(L3C::format_t format = L3C::FORMAT_RAW)
		{
			memset(&_packet, 0x00, L3C::SECURITY_PACKET_LEN_MIN);

			_packet.format = format;
			_packet.payload_len = _GetFormat().overhead;

			_param.error = L3C::ERROR_NONE;
			_param.parsed = false;
			_param.prepared = false;
			
			return;
		}
		
	private:

		const format_info_t &_GetFormat() const
		{
			static constexpr format_info_t invalid = {0, 0, 0, (L3C::format_t)0};

			uint8_t format = (uint8_t)_packet.format;

			if(format >= sizeof(_format_info) / sizeof(_format_info[0]))
				return invalid;

			return _format_info[format];
		}

		uint16_t _PayloadLength() const
		{
			const format_info_t &format = _GetFormat();

			if(_packet.payload_len < format.overhead)
				return 0;

			return _packet.payload_len - format.overhead;
		}

		uint8_t *_PayloadData()
		{
			return &_packet.payload[_GetFormat().data_offset];
		}

		uint8_t *_IV()
		{
			return _packet.payload;
		}

		uint8_t *_Tag()
		{
			return &_packet.payload[_GetFormat().data_offset + _PayloadLength()];
		}

		void _Prepare()
		{
			if(_param.prepared)
				return;
			
			const format_info_t &format = _GetFormat();

			switch(format.format)
			{
				case L3C::FORMAT_RAW:
				{
					_param.prepared = true;
					break;
				}

				case L3C::FORMAT_SIGN:
				{
					if(_Signing())
						_param.prepared = true;
					else
						_SetError(L3C::ERROR_SIGN);
					break;
				}

				case L3C::FORMAT_ENCRYPT:
				{
					if(_Encrypting())
						_param.prepared = true;
					else
						_SetError(L3C::ERROR_ENCRYPT);
					break;
				}

				default:
				{
					_SetError(L3C::ERROR_FORMAT);
					break;
				}
			}

			return;
		}
		
		void _Parse()
		{
			const format_info_t &format = _GetFormat();

			_param.parsed = false;

			if(format.capacity == 0)
				return _SetError(L3C::ERROR_FORMAT);

			if(_packet.payload_len < format.overhead)
				return _SetError(L3C::ERROR_MIN_LEN);

			if(_PayloadLength() > format.capacity)
				return _SetError(L3C::ERROR_LEN);

			switch(format.format)
			{
				case L3C::FORMAT_RAW:
				{
					_param.error = L3C::ERROR_NONE;
					_param.parsed = true;
					break;
				}

				case L3C::FORMAT_SIGN:
				{
					if(_Verifying())
					{
						_param.error = L3C::ERROR_NONE;
						_param.parsed = true;
					}
					break;
				}

				case L3C::FORMAT_ENCRYPT:
				{
					if(_Decrypting())
					{
						_param.error = L3C::ERROR_NONE;
						_param.parsed = true;
					}
					break;
				}

				default:
				{
					_SetError(L3C::ERROR_FORMAT);
					break;
				}
			}

			return;
		}
		
		void _SetError(L3C::error_t error)
		{
			_param.error = error;
			_param.parsed = false;
			return;
		}
		
		bool _Signing()
		{
			mbedtls_chachapoly_context ctx;
			
			mbedtls_chachapoly_init(&ctx);
			if(mbedtls_chachapoly_setkey(&ctx, _param.key) != 0)
			{
				mbedtls_chachapoly_free(&ctx);
				return false;
			}

			uint8_t *aadata = (uint8_t *)&_packet;
			uint16_t aadata_len = L3C::SECURITY_PACKET_LEN_MIN + L3C::SECURITY_IV_LEN + _PayloadLength();

			(*(uint32_t *)&_packet.payload[8])++;

			int ret = mbedtls_chachapoly_encrypt_and_tag(
				&ctx,
				0,
				_IV(),
				aadata,
				aadata_len,
				nullptr,
				nullptr,
				_Tag());

			mbedtls_chachapoly_free(&ctx);
			
			return (ret == 0);
		}
		
		bool _Verifying()
		{
			mbedtls_chachapoly_context ctx;
			
			mbedtls_chachapoly_init(&ctx);
			if(mbedtls_chachapoly_setkey(&ctx, _param.key) != 0)
			{
				mbedtls_chachapoly_free(&ctx);
				return false;
			}
			
			uint8_t *aadata = (uint8_t *)&_packet;
			uint16_t aadata_len = L3C::SECURITY_PACKET_LEN_MIN + L3C::SECURITY_IV_LEN + _PayloadLength();

			int ret = mbedtls_chachapoly_auth_decrypt(
				&ctx,
				0,
				_IV(),
				aadata,
				aadata_len,
				_Tag(),
				nullptr,
				nullptr);

			if(ret == MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED)
				_SetError(L3C::ERROR_AUTH_FAILED);

			mbedtls_chachapoly_free(&ctx);
			
			return (ret == 0);
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
			uint16_t aadata_len = L3C::SECURITY_PACKET_LEN_MIN + L3C::SECURITY_IV_LEN;
			uint16_t data_len = _PayloadLength();

			(*(uint32_t *)&_packet.payload[8])++;

			int ret = mbedtls_chachapoly_encrypt_and_tag(
				&ctx,
				data_len,
				_IV(),
				aadata,
				aadata_len,
				_PayloadData(),
				_PayloadData(),
				_Tag());
			
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
			uint16_t aadata_len = L3C::SECURITY_PACKET_LEN_MIN + L3C::SECURITY_IV_LEN;
			uint16_t data_len = _PayloadLength();

			int ret = mbedtls_chachapoly_auth_decrypt(
				&ctx,
				data_len,
				_IV(),
				aadata,
				aadata_len,
				_Tag(),
				_PayloadData(),
				_PayloadData());

			if(ret == MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED)
				_SetError(L3C::ERROR_AUTH_FAILED);
			
			mbedtls_chachapoly_free(&ctx);
			
			return (ret == 0);
		}
		
		L3C::security_t _packet;

		struct
		{
			uint32_t last_rx_time;
			uint16_t timeout;
			uint8_t *key;
			uint8_t type;
			L3C::error_t error;
			bool parsed;
			bool prepared;
		} _param;
};
