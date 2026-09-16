#pragma once
#include <inttypes.h>

class L3ManagerInterface
{
	public:
		virtual ~L3ManagerInterface() = default;
		
		virtual uint16_t RxPacket(uint8_t dev_type, const uint8_t *rx, uint16_t rx_len) = 0;
};
