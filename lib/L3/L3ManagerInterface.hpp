#pragma once
#include <inttypes.h>

class L3Driver;

class L3ManagerInterface
{
	public:
		virtual ~L3ManagerInterface() = default;
		
		virtual uint16_t RxPacket(L3Driver *driver, const uint8_t *rx, uint16_t rx_len) = 0;
};
