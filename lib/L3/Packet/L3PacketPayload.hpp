#pragma once
#include <inttypes.h>
#include <string.h>
#include "L3PacketConsts.hpp"
#include <CUtils_RingBuffer.h>

namespace L3C = L3PacketConsts;

struct L3PacketPayloadObj_t
{
	uint16_t length;
	uint8_t data[L3C::SECURITY_PACKET_LEN_MAX];
};

class L3PacketPayload final : private RingBuffer<64, L3PacketPayloadObj_t>
{
	public:

		bool Add(const uint8_t *data, uint16_t length)
		{
			if(length > L3C::SECURITY_PACKET_LEN_MAX) return false;
			
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
			return DeleteOne();
		}

		void Init()
		{
			Clear();

			return;
		}

	private:

		inline L3PacketPayloadObj_t *GetWriteSlot() noexcept
		{
			if(IsFull()) return nullptr;
				
			return &_data[_head];
		}

		inline void CommitWriteSlot() noexcept
		{
			_head = nextIndex(_head);

			return;
		}

		inline L3PacketPayloadObj_t *GetReadSlot() noexcept
		{
			if(IsFull()) return nullptr;
			
			return &_data[_tail];
		}
};
