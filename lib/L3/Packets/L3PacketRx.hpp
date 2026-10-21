#pragma once
#include <inttypes.h>
#include <string.h>
#include "L3Packet.hpp"
#include "L3PacketConsts.hpp"
#include "L3PacketPayload.hpp"

namespace L3C = L3PacketConsts;

class L3PacketRx final : private L3Packet
{
	public:

		using L3Packet::CfgKey;
		using L3Packet::IsError;
		using L3Packet::GetError;

		L3PacketRx()
		{
			_allowed = L3C::FORMATS_SECURE;
			_timeout = 0;
			_last_rx_time = 0;
			_dropped = 0;
			_ResetFrame();
		}

		// Таймаут неактивности при побайтовом приёме, мс. 0 — выключен
		void CfgTimeout(uint16_t timeout)
		{
			_timeout = timeout;
			return;
		}

		// Маска разрешённых форматов (1 << FORMAT_x). По умолчанию только SIGN и ENCRYPT.
		// RAW не аутентифицирован: разрешать только если потребитель считает такие данные недоверенными
		void CfgFormats(uint8_t mask)
		{
			_allowed = mask;
			return;
		}

		void Reset()
		{
			_ResetFrame();
			memset(&_packet, 0x00, L3C::PACKET_HEADER_LEN);
			_param.error = L3C::ERROR_NONE;
			_payload.Clear();

			return;
		}

		// Кадр целиком (с копированием)
		bool PutPacketBytes(const uint8_t *data, uint16_t length, uint32_t time)
		{
			_CheckTimeout(time);

			if(length > L3C::PACKET_LEN_MAX) return _SetError(L3C::ERROR_RX_FRAME_TOO_LONG);
			if(length < L3C::PACKET_LEN_MIN) return _SetError(L3C::ERROR_RX_FRAME_TOO_SHORT);

			memcpy(&_packet, data, length);
			_put_idx = length;

			bool ok = _Parse();
			_ResetFrame();

			return ok;
		}

		// Один байт при побайтовом приёме. После каждого завершённого или битого кадра разбор начинается заново
		bool AddPacketByte(uint8_t data, uint32_t time)
		{
			_CheckTimeout(time);

			// Синхронизация по сигнатуре
			if(_put_idx == 0 && data != L3C::PACKET_HEAD_0) return _SetError(L3C::ERROR_RX_HEAD);
			if(_put_idx == 1 && data != L3C::PACKET_HEAD_1)
			{
				_ResetFrame();
				return _SetError(L3C::ERROR_RX_HEAD);
			}

			((uint8_t *)&_packet)[_put_idx++] = data;

			if(_put_idx == L3C::PACKET_HEADER_LEN)
			{
				if(_packet.payload_len > L3C::PACKET_PAYLOAD_RAW_LEN)
				{
					_ResetFrame();
					return _SetError(L3C::ERROR_RX_PAYLOAD_LEN_MAX);
				}

				_frame_len = _PacketLength() + L3C::PACKET_CRC_LEN;		// не больше PACKET_LEN_MAX
			}

			if(_put_idx == _frame_len)
			{
				bool ok = _Parse();
				_ResetFrame();

				return ok;
			}

			return true;
		}

		// Приём без копирования (например, из DMA/UART): получить буфер размером length...
		bool RxPtrStart(uint8_t *&data, uint16_t length, uint32_t time)
		{
			_CheckTimeout(time);

			if(length > L3C::PACKET_LEN_MAX) return _SetError(L3C::ERROR_RX_FRAME_TOO_LONG);
			if(length < L3C::PACKET_LEN_MIN) return _SetError(L3C::ERROR_RX_FRAME_TOO_SHORT);

			data = (uint8_t *)&_packet;

			return true;
		}

		// ...и после заполнения сообщить, сколько байт реально записано
		bool RxPtrEnd(uint16_t received)
		{
			_put_idx = received;

			bool ok = _Parse();
			_ResetFrame();

			return ok;
		}

		bool IsReady()
		{
			return _payload.IsEmpty() == false;
		}

		// Самый старый готовый payload без удаления. Длина всегда > 0
		uint16_t GetPayloadPtr(uint8_t *&data)
		{
			return _payload.Get(data);
		}

		// Формат этого payload: RAW — не аутентифицирован
		L3C::format_t GetPayloadFormat()
		{
			return _payload.GetFormat();
		}

		bool PopPayload()
		{
			return _payload.Delete();
		}

		uint32_t GetDroppedCount()
		{
			return _dropped;
		}

	private:

		void _ResetFrame()
		{
			_put_idx = 0;
			_frame_len = 0;

			return;
		}

		// Таймаут сбрасывает только разбор текущего кадра, очередь принятых payload не трогает
		void _CheckTimeout(uint32_t time)
		{
			if(_timeout && (time - _last_rx_time) >= _timeout)
				_ResetFrame();
			_last_rx_time = time;

			return;
		}

		bool _Parse()
		{
			if(_put_idx < L3C::PACKET_LEN_MIN) return _SetError(L3C::ERROR_RX_FRAME_TOO_SHORT);
			if(_packet.head[0] != L3C::PACKET_HEAD_0 || _packet.head[1] != L3C::PACKET_HEAD_1) return _SetError(L3C::ERROR_RX_HEAD);
			if(_packet.payload_len > L3C::PACKET_PAYLOAD_RAW_LEN) return _SetError(L3C::ERROR_RX_PAYLOAD_LEN_MAX);
			if(_put_idx != _PacketLength() + L3C::PACKET_CRC_LEN) return _SetError(L3C::ERROR_RX_LEN_MISMATCH);

			// CRC — дешёвый фильтр до крипто, не защита от подделки
			if(_ReadCRC() != _ComputeCRC()) return _SetError(L3C::ERROR_RX_CRC);

			if(_packet.version != L3C::PACKET_VERSION) return _SetError(L3C::ERROR_RX_VERSION);
			if(_packet.direction != L3C::DIRECTION_TO) return _SetError(L3C::ERROR_RX_DIRECTION);

			const format_info_t &f = _Info();

			if(f.capacity == 0) return _SetError(L3C::ERROR_RX_FORMAT_RESERVED);
			if((_allowed & (1U << _packet.format)) == 0) return _SetError(L3C::ERROR_RX_FORMAT_DENIED);
			if(_packet.payload_len < f.overhead) return _SetError(L3C::ERROR_RX_PAYLOAD_LEN_MIN);
			if(_PayloadLength() > f.capacity) return _SetError(L3C::ERROR_RX_DATA_LEN);

			if(f.overhead)		// SIGN / ENCRYPT
			{
				if(_param.key == nullptr) return _SetError(L3C::ERROR_CFG_NO_KEY);
				if(_seq == nullptr) return _SetError(L3C::ERROR_CFG_NO_SEQ);
				if(_seq->IsStarted() == false) return _SetError(L3C::ERROR_SEQ_NOT_STARTED);

				// Партнёр обязан инвертировать instance — этим разведены IV двух направлений
				if(_IV()[3] != (uint8_t)~_param.type) return _SetError(L3C::ERROR_RX_IV_INSTANCE);

				uint64_t n = _IVGetN();
				if(_seq->IsFresh(n) == false) return _SetError(L3C::ERROR_RX_IV_REPLAY);

				int ret = _Gcm(false);
				if(ret != 0)
					return _SetError((ret == MBEDTLS_ERR_GCM_AUTH_FAILED) ? L3C::ERROR_RX_AUTH : L3C::ERROR_RX_CRYPTO);

				_seq->Accept(n);	// только после успешной проверки тега
			}

			_param.error = L3C::ERROR_NONE;

			uint16_t length = _PayloadLength();
			if(length == 0) return true;	// валидный пакет без данных (keepalive): в очередь не кладём

			if(_payload.Add(_PayloadData(), length, (L3C::format_t)_packet.format) == false)
			{
				_dropped++;
				return _SetError(L3C::ERROR_RX_QUEUE_FULL);
			}

			return true;
		}

		uint16_t _put_idx;			// Сколько байт кадра принято
		uint16_t _frame_len;		// Ожидаемая длина кадра, известна после заголовка (побайтовый приём)
		uint32_t _last_rx_time;
		uint16_t _timeout;
		uint8_t _allowed;
		uint32_t _dropped;

		L3PacketPayload _payload;
};
