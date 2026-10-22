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
			_format = L3C::FORMAT_SIGN;
			_timeout = 0;
			_last_rx_time = 0;
			_dropped = 0;
			_valid = 0;
			_ResetFrame();
		}

		// Таймаут неактивности при побайтовом приёме, мс. 0 — выключен
		void CfgTimeout(uint16_t timeout)
		{
			_timeout = timeout;
			return;
		}

		// Формат канала: принимается только он, остальные кадры отбрасываются (ERROR_RX_FORMAT_DENIED).
		// По умолчанию SIGN. RAW не аутентифицирован: только для каналов, где целостность даёт транспорт (TLS)
		void CfgFormat(L3C::format_t format)
		{
			_format = format;
			return;
		}

		// Вызывать, когда задача приёма остановлена: очередь payload не защищена от одновременной записи
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

		// Кусок потока (UART/USB): в нём может быть несколько кадров подряд, начало или конец кадра,
		// мусор между кадрами. Кадр, не дособранный до конца куска, продолжится со следующим куском.
		// Данные копируются блоками, побайтно обрабатывается только поиск сигнатуры.
		// Если кандидат в кадр оказался мусором (длина вне диапазона, CRC не сошёлся), его байты со второго
		// просматриваются заново: внутри мог начинаться настоящий кадр.
		// Возвращает число кадров, прошедших все проверки; ошибка последнего отбракованного в этом куске — GetError()
		uint16_t PutStream(const uint8_t *data, uint16_t length, uint32_t time)
		{
			_CheckTimeout(time);
			_param.error = L3C::ERROR_NONE;

			uint8_t *frame = (uint8_t *)&_packet;
			uint16_t accepted = 0;
			L3C::error_t rejected = L3C::ERROR_NONE;	// успешный кадр не должен затирать ошибку предыдущего

			// Байты на повторный просмотр. Помещаются всегда: это хвост одного отбракованного кадра
			uint8_t back[L3C::PACKET_LEN_MAX];
			uint16_t back_len = 0, back_pos = 0;

			while(true)
			{
				const uint8_t *src;
				uint16_t avail;
				bool from_back = (back_pos < back_len);

				if(from_back) { src = back + back_pos; avail = back_len - back_pos; }
				else if(length > 0) { src = data; avail = length; }
				else break;

				uint16_t used;
				bool rescan = false;

				if(_put_idx == 0)
				{
					// Поиск 'P'
					const uint8_t *p = (const uint8_t *)memchr(src, L3C::PACKET_HEAD_0, avail);
					if(p == nullptr)
					{
						used = avail;
					}
					else
					{
						used = (uint16_t)(p - src) + 1;
						frame[_put_idx++] = L3C::PACKET_HEAD_0;
					}
				}
				else if(_put_idx == 1)
				{
					used = 1;
					if(src[0] == L3C::PACKET_HEAD_1)
						frame[_put_idx++] = src[0];
					else if(src[0] != L3C::PACKET_HEAD_0)	// 'P','P','X' — вторая 'P' может быть началом кадра
						_ResetFrame();
				}
				else
				{
					// Заголовок, затем остаток кадра — блоками
					uint16_t target = (_frame_len == 0) ? L3C::PACKET_HEADER_LEN : _frame_len;
					used = target - _put_idx;
					if(used > avail) used = avail;

					memcpy(frame + _put_idx, src, used);
					_put_idx += used;

					if(_frame_len == 0 && _put_idx == L3C::PACKET_HEADER_LEN)
					{
						if(_packet.payload_len > L3C::PACKET_PAYLOAD_RAW_LEN)
						{
							rejected = L3C::ERROR_RX_PAYLOAD_LEN_MAX;
							rescan = true;
						}
						else
						{
							_frame_len = _PacketLength() + L3C::PACKET_CRC_LEN;		// не больше PACKET_LEN_MAX
						}
					}

					if(rescan == false && _frame_len != 0 && _put_idx == _frame_len)
					{
						if(_Parse())
						{
							accepted++;
							_ResetFrame();
						}
						else if(_param.error == L3C::ERROR_RX_CRC)
						{
							rejected = _param.error;
							rescan = true;		// кадра не было: искать сигнатуру внутри
						}
						else
						{
							rejected = _param.error;
							_ResetFrame();		// кадр целый, но отклонён (повтор, подпись...): внутри искать нечего
						}
					}
				}

				if(from_back) back_pos += used;
				else { data += used; length -= used; }

				if(rescan)
				{
					// Новая очередь просмотра: байты кандидата со второго + ещё не просмотренный хвост старой
					uint16_t n = _put_idx - 1;
					uint16_t rest = back_len - back_pos;
					if(n + rest > sizeof(back)) rest = 0;	// не бывает (n + rest < PACKET_LEN_MAX), страховка
					memmove(back + n, back + back_pos, rest);
					memcpy(back, frame + 1, n);
					back_len = n + rest;
					back_pos = 0;
					_ResetFrame();
				}
			}

			_param.error = rejected;

			return accepted;
		}

		// Сбросить недособранный кадр (например, после переполнения буфера UART)
		void DropFrame()
		{
			_ResetFrame();

			return;
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

		bool PopPayload()
		{
			return _payload.Delete();
		}

		uint32_t GetDroppedCount()
		{
			return _dropped;
		}

		// Счётчик принятых аутентифицированных пакетов (SIGN/ENCRYPT, прошедших все проверки),
		// включая пустые. Только растёт. Драйвер по его изменению видит активность клиента.
		// Пишет задача приёма, читает runtime: 32-битное выровненное чтение атомарно
		uint32_t GetValidCount() const
		{
			return _valid;
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
			if(_packet.format != _format) return _SetError(L3C::ERROR_RX_FORMAT_DENIED);
			if(_packet.payload_len < f.overhead) return _SetError(L3C::ERROR_RX_PAYLOAD_LEN_MIN);
			if(_PayloadLength() > f.capacity) return _SetError(L3C::ERROR_RX_DATA_LEN);

			if(f.overhead)		// SIGN / ENCRYPT
			{
				if(_param.key == nullptr) return _SetError(L3C::ERROR_CFG_NO_KEY);
				if(_seq == nullptr) return _SetError(L3C::ERROR_CFG_NO_SEQ);
				if(_seq->IsStarted() == false) return _SetError(L3C::ERROR_SEQ_NOT_STARTED);

				// Партнёр обязан инвертировать instance — этим разведены IV двух направлений
				if(_IV()->instance != (uint8_t)~_param.type) return _SetError(L3C::ERROR_RX_IV_INSTANCE);

				uint64_t n = _IV()->GetN();
				if(_seq->IsFresh(n) == false) return _SetError(L3C::ERROR_RX_IV_REPLAY);

				uint8_t session[L3C::PACKET_AAD_BIND_LEN];
				_seq->Session(session);
				int ret = _Gcm(false, session);	// клиент подписал и идентификатор текущего сеанса
				if(ret != 0)
					return _SetError((ret == MBEDTLS_ERR_GCM_AUTH_FAILED) ? L3C::ERROR_RX_AUTH : L3C::ERROR_RX_CRYPTO);

				_seq->Accept(n);	// только после успешной проверки тега
				_valid = _valid + 1;	// не ++: инкремент volatile устарел в C++20
			}

			_param.error = L3C::ERROR_NONE;

			uint16_t length = _PayloadLength();
			if(length == 0) return true;	// валидный пакет без данных (keepalive): в очередь не кладём

			if(_payload.Add(_PayloadData(), length) == false)
			{
				_dropped = _dropped + 1;
				return _SetError(L3C::ERROR_RX_QUEUE_FULL);
			}

			return true;
		}

		uint16_t _put_idx;			// Сколько байт кадра принято
		uint16_t _frame_len;		// Ожидаемая длина кадра, известна после заголовка (побайтовый приём)
		uint32_t _last_rx_time;
		uint16_t _timeout;
		L3C::format_t _format;		// формат канала: только он принимается
		volatile uint32_t _dropped;
		volatile uint32_t _valid;

		L3PacketPayload _payload;
};
