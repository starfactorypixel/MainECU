/*
	Драйвера физического уровня для протокола L3.
*/

#pragma once

class L3Driver
{
	public:
		virtual void Init() = 0;
		virtual uint8_t ReadAvailable() = 0;
		virtual uint8_t ReadByte() = 0;
		virtual void SendByte(uint8_t data) = 0;
};

#ifdef ARDUINO_ARCH_ESP32
	#include "L3DriverBluetooth.h"
	#include "L3DriverSerial.h"
	//#include "L3DriverSoftSerial.h"
	#include "L3DriverUART.h"
#elif ARDUINO_ARCH_AVR
	#include "L3DriverSerial.h"
	#include "L3DriverSoftSerial.h"
	#include "L3DriverUART.h"
#endif
