// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
// https://github.com/sandeepmistry/arduino-CAN
// Porting for StarPixel project: Dragon_Knight, https://github.com/Dragon-Knight

#pragma once
#include <Arduino.h>					// yield();
#include <functional>
#include "esp_chip_info.h"
//#include "esp_intr_alloc.h"
//#include "soc/dport_reg.h"
#include "driver/gpio.h"
//#include "string.h"

// ESP32-S3 Check for both ESP-IDF and Arduino environments
#if defined(ESP_PLATFORM) && (__has_include("esp_intr_alloc.h")) 
	#include "esp_intr_alloc.h"
	#include "rom/gpio.h"
	#include "soc/interrupts.h"
	#include "soc/system_reg.h"
	#define ARDUINO_ESP32S3
	#define CAN_RX_IDX TWAI_RX_IDX
	#define CAN_TX_IDX TWAI_TX_IDX
	#define ETS_CAN_INTR_SOURCE ETS_TWAI_INTR_SOURCE
	#define DR_REG_CAN_BASE DR_REG_TWAI_BASE

#elif defined(ESP_PLATFORM) && (__has_include("esp_intr.h"))
	#include "esp_intr.h"
	#include "soc/dport_reg.h"

#else
	#error "Unsupported ESP32 variant or missing header files"
#endif

class ESP32SJA1000Class
{
	static constexpr uint32_t NO_CAN_ID = 0xFFFFFFFF;
	
	public:	
		
		struct packet_new_t
		{
			bool flag = false;			// Некий флаг для работы, для tx это инициализация пакета, для rx это готовность пакета
			bool extended = false;		// Флаг exID
			bool rtr = false;			// Флаг RTR
			uint32_t id = NO_CAN_ID;	// Идентификатор пакета CAN
			uint8_t dlc = 0;			// DLC ??
			uint8_t length = 0;			// Длина пакета CAN
			uint8_t data[8] = {};		// Данные пакета CAN
		};
		
		typedef std::function<void(packet_new_t &packet)> on_receive_t;
		//using on_receive_t = void (*)(packet_new_t &packet);
		
		ESP32SJA1000Class() : 
			_rxPin(GPIO_NUM_4), _txPin(GPIO_NUM_5),
			_rx{}, _tx{},
			_onReceive(nullptr),
			_loopback(false),
			_intrHandle(nullptr)
		{}
		
		void setCallback(on_receive_t callback);
		void setPins(gpio_num_t rx, gpio_num_t tx);
		bool begin(uint32_t baudRate);
		void end();
		
		bool beginPacket(uint16_t id, bool rtr = false);
		bool beginExtendedPacket(uint32_t id, bool rtr = false);
		uint8_t write(uint8_t byte){ return write(&byte, 1); }
		uint8_t write(const uint8_t *buffer, uint8_t size);
		bool endPacket();
		bool SendPacket(packet_new_t &packet);
		
		bool filter(uint16_t id) { return filter(id, 0x07FF); }
		bool filter(uint16_t id, uint16_t mask);
		bool filterExtended(uint32_t id) { return filterExtended(id, 0x1FFFFFFF); }
		bool filterExtended(uint32_t id, uint32_t mask);
		
		void cmd_reset();
		void cmd_observe();
		void cmd_loopback();
		void cmd_sleep();
		void cmd_wakeup();
		
	private:
		
		bool parsePacket();
		
		static void onInterrupt(void *arg);
		void handleInterrupt();
		
		uint32_t readRegister(uint32_t address);
		void modifyRegister(uint32_t address, uint32_t mask, uint32_t value);
		void writeRegister(uint32_t address, uint32_t value);
		bool writeReadRegister(uint32_t address, uint32_t value);
		
		
		gpio_num_t _rxPin;
		gpio_num_t _txPin;
		
		packet_new_t _rx;
		packet_new_t _tx;
		
		on_receive_t _onReceive;
		
		bool _loopback;
		intr_handle_t _intrHandle;

};
