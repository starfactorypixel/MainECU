#pragma once
#include <inttypes.h>
#include <string.h>
#include "L3PacketConsts.hpp"
#include <CUtils_RingBuffer.h>

namespace L3C = L3PacketConsts;

struct L3PacketPayloadObj_t
{
	uint16_t length;
	uint8_t data[L3C::PACKET_PAYLOAD_RAW_LEN];
};

class L3PacketPayload final : private RingBuffer<64, L3PacketPayloadObj_t>
{
	public:
	
		using RingBuffer::IsEmpty;
		using RingBuffer::IsFull;
		using RingBuffer::Count;
		using RingBuffer::Clear;

		bool Add(const uint8_t *data, uint16_t length)
		{
			if(length == 0 || data == nullptr) return false;
			if(length > L3C::PACKET_PAYLOAD_RAW_LEN) return false;

			auto *obj = GetWriteSlot();
			if(!obj) return false;

			obj->length = length;
			memcpy(obj->data, data, length);
			CommitWriteSlot();

			return true;
		}

		uint16_t Get(uint8_t *&data)
		{
			auto *obj = GetReadSlot();
			if(!obj) return 0;

			data = obj->data;
			return obj->length;
		}

		bool Delete()
		{
			__atomic_thread_fence(__ATOMIC_RELEASE);	// Данные слота дочитаны до того, как слот освобождается
			return DeleteOne();
		}

	private:

		inline L3PacketPayloadObj_t *GetWriteSlot() noexcept
		{
			if(IsFull()) return nullptr;
			__atomic_thread_fence(__ATOMIC_ACQUIRE);	// Слот освобождён читателем — только потом в него пишем

			return &_data[_head];
		}

		inline void CommitWriteSlot() noexcept
		{
			__atomic_thread_fence(__ATOMIC_RELEASE);	// Данные слота записаны до сдвига _head
			_head = nextIndex(_head);

			return;
		}

		inline L3PacketPayloadObj_t *GetReadSlot() noexcept
		{
			if(IsEmpty()) return nullptr;
			__atomic_thread_fence(__ATOMIC_ACQUIRE);	// Сдвиг _head увиден — данные слота читаем после него

			return &_data[_tail];
		}
};
