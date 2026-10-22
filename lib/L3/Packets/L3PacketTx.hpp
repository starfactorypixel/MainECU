#pragma once
#include <inttypes.h>
#include <string.h>
#include "L3Packet.hpp"
#include "L3PacketConsts.hpp"

namespace L3C = L3PacketConsts;

class L3PacketTx final : private L3Packet
{
	public:

		using L3Packet::CfgKey;
		using L3Packet::IsError;
		using L3Packet::GetError;

		L3PacketTx()
		{
			_format = L3C::FORMAT_RAW;
			Clear();
		}

		// Задать формат и очистить пакет
		void CfgFormat(L3C::format_t format)
		{
			_format = format;
			Clear();

			return;
		}

		// Очистить пакет, формат сохраняется. Состояние счётчиков лежит в L3PacketSeq и не затрагивается
		void Clear()
		{
			memset(&_packet, 0x00, L3C::PACKET_HEADER_LEN);

			_packet.head[0] = L3C::PACKET_HEAD_0;
			_packet.head[1] = L3C::PACKET_HEAD_1;
			_packet.version = L3C::PACKET_VERSION;
			_packet.direction = L3C::DIRECTION_FROM;
			_packet.format = _format;
			_packet.payload_len = _Info().overhead;

			_param.error = (_Info().capacity == 0) ? L3C::ERROR_CFG_FORMAT : L3C::ERROR_NONE;
			_sealed = false;

			return;
		}

		L3C::format_t GetFormat()
		{
			return _format;
		}

		// Записать данные, затирая накопленные
		bool PutPayload(const uint8_t *data, uint16_t length)
		{
			const format_info_t &f = _Info();

			if(_sealed) return _SetError(L3C::ERROR_TX_SEALED);
			if(f.capacity == 0) return _SetError(L3C::ERROR_CFG_FORMAT);
			if(length > f.capacity) return _SetError(L3C::ERROR_TX_DATA_TOO_BIG);

			if(length)
				memcpy(&_packet.payload[f.data_offset], data, length);

			_packet.payload_len = f.overhead + length;
			_param.error = L3C::ERROR_NONE;

			return true;
		}

		// Дописать данные к накопленным.
		// ERROR_TX_NO_SPACE — в этом пакете нет места, нужен Commit и повтор.
		// ERROR_TX_DATA_TOO_BIG — не поместится никогда, повторять бессмысленно
		bool AddPayload(const uint8_t *data, uint16_t length)
		{
			const format_info_t &f = _Info();
			uint16_t used = _PayloadLength();

			if(_sealed) return _SetError(L3C::ERROR_TX_SEALED);
			if(f.capacity == 0) return _SetError(L3C::ERROR_CFG_FORMAT);
			if(length > f.capacity) return _SetError(L3C::ERROR_TX_DATA_TOO_BIG);
			if(length > f.capacity - used) return _SetError(L3C::ERROR_TX_NO_SPACE);

			if(length)
				memcpy(&_packet.payload[f.data_offset + used], data, length);

			_packet.payload_len = f.overhead + used + length;
			_param.error = L3C::ERROR_NONE;

			return true;
		}

		bool PayloadIsSet()
		{
			return (_PayloadLength() > 0);
		}

		// Подписать/зашифровать (один раз) и вернуть готовый кадр. 0 — ошибка, см. GetError().
		// Повторный вызов отдаёт тот же кадр с тем же N
		uint16_t GetPacketPtr(uint8_t *&data)
		{
			if(_sealed == false && _Seal() == false)
				return 0;

			data = (uint8_t *)&_packet;

			return _PacketLength() + L3C::PACKET_CRC_LEN;
		}

	private:

		bool _Seal()
		{
			const format_info_t &f = _Info();

			if(f.capacity == 0) return _SetError(L3C::ERROR_CFG_FORMAT);

			if(f.overhead)		// SIGN / ENCRYPT
			{
				if(_param.key == nullptr) return _SetError(L3C::ERROR_CFG_NO_KEY);
				if(_seq == nullptr) return _SetError(L3C::ERROR_CFG_NO_SEQ);

				uint64_t n;
				L3C::error_t err = _seq->NextTx(n);
				if(err != L3C::ERROR_NONE) return _SetError(err);

				_IVWrite(n);

				if(_Gcm(true) != 0) return _SetError(L3C::ERROR_TX_CRYPTO);
			}

			_WriteCRC(_ComputeCRC());	// последним, после подписи: позиция сразу за данными
			_sealed = true;

			return true;
		}

		// Раскладка IV — L3C::IV_t: [epoch:24][instance:8][counter:32][random:32]
		void _IVWrite(uint64_t n)
		{
			L3C::IV_t *iv = _IV();

			iv->SetN(n);
			iv->instance = _param.type;
			_seq->Session(iv->random);			// у ESP поле random IV — идентификатор сеанса

			return;
		}

		L3C::format_t _format;
		bool _sealed;
};
