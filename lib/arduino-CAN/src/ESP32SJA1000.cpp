// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
// https://github.com/sandeepmistry/arduino-CAN
// Porting for StarPixel project: Dragon_Knight, https://github.com/Dragon-Knight

#include "ESP32SJA1000.h"

// esp32\include\soc\esp32\include\soc\twai_struct.h
// esp32s3\include\soc\esp32s3\include\soc\twai_struct.h
#define REG_MOD			0x00			// mode_reg
#define REG_CMR			0x01			// command_reg
#define REG_SR			0x02			// status_reg
#define REG_IR			0x03			// interrupt_reg
#define REG_IER			0x04			// interrupt_enable_reg
#define REG_BTR0		0x06			// bus_timing_0_reg
#define REG_BTR1		0x07			// bus_timing_1_reg
#define REG_ALC			0x0B			// arbitration_lost_captue_reg
#define REG_ECC			0x0C			// error_code_capture_reg
#define REG_EWLR		0x0D			// error_warning_limit_reg
#define REG_RXERR		0x0E			// rx_error_counter_reg
#define REG_TXERR		0x0F			// tx_error_counter_reg
#define REG_SFF			0x10			// tx_rx_buffer[12]
#define REG_EFF			0x10			// tx_rx_buffer[12]
#define REG_ACRn(n)		(0x10 + n)	// acceptance_filter > acr[4]
#define REG_AMRn(n)		(0x14 + n)	// acceptance_filter > amr[4]
#define REG_RXMC		0x1D			// rx_message_counter_reg
#define REG_CDR			0x1F			// clock_divider_reg


void ESP32SJA1000Class::setCallback(on_receive_t callback)
{
	_onReceive = callback;
	
	if(_intrHandle)
	{
		esp_intr_free(_intrHandle);
		_intrHandle = NULL;
	}
	esp_intr_alloc(ETS_CAN_INTR_SOURCE, 0, ESP32SJA1000Class::onInterrupt, this, &_intrHandle);
	
	return;
}

void ESP32SJA1000Class::setPins(gpio_num_t rx, gpio_num_t tx)
{
	_rxPin = rx;
	_txPin = tx;
	
	return;
}

bool ESP32SJA1000Class::begin(uint32_t baudRate)
{
	// Нет необходимости чистить всё полностью. Этого достаточно
	_rx.flag = false;
	_rx.id = NO_CAN_ID;
	_tx.flag = false;
	_tx.id = NO_CAN_ID;
	
	_loopback = false;

#ifdef ARDUINO_ESP32S3
	CLEAR_PERI_REG_MASK(SYSTEM_PERIP_RST_EN0_REG, SYSTEM_TWAI_RST);
	SET_PERI_REG_MASK(SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_TWAI_CLK_EN);
#else
	DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_CAN_RST);
	DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_CAN_CLK_EN);
#endif
	
	// Входим в режим reset
	modifyRegister(REG_MOD, 0x01, 0x01);

	gpio_set_direction(_rxPin, GPIO_MODE_INPUT);
	gpio_matrix_in(_rxPin, CAN_RX_IDX, 0);
	gpio_pad_select_gpio(_rxPin);

	gpio_set_direction(_txPin, GPIO_MODE_OUTPUT);
	gpio_matrix_out(_txPin, CAN_TX_IDX, 0, 0);
	gpio_pad_select_gpio(_txPin);

#ifndef ARDUINO_ESP32S3

	modifyRegister(REG_CDR, 0x80, 0x80);	// Pelican mode
	modifyRegister(REG_BTR0, 0xC0, 0x40);	// SJW = 1
	modifyRegister(REG_BTR1, 0x70, 0x10);	// TSEG2 = 1
	modifyRegister(REG_BTR1, 0x80, 0x80);	// SAM = 1
	
	switch(baudRate)
	{
		case 1000000:
			modifyRegister(REG_BTR1, 0x0F, 0x04);
			modifyRegister(REG_BTR0, 0x3F, 4);
			break;
		
		case 500000:
			modifyRegister(REG_BTR1, 0x0F, 0x0C);
			modifyRegister(REG_BTR0, 0x3F, 4);
			break;
		
		case 250000:
			modifyRegister(REG_BTR1, 0x0F, 0x0C);
			modifyRegister(REG_BTR0, 0x3F, 9);
			break;
		
		case 200000:
			modifyRegister(REG_BTR1, 0x0F, 0x0C);
			modifyRegister(REG_BTR0, 0x3F, 12);
			break;
		
		case 125000:
			modifyRegister(REG_BTR1, 0x0F, 0x0C);
			modifyRegister(REG_BTR0, 0x3F, 19);
			break;
		
		case 100000:
			modifyRegister(REG_BTR1, 0x0F, 0x0C);
			modifyRegister(REG_BTR0, 0x3F, 24);
			break;

		case 80000:
			modifyRegister(REG_BTR1, 0x0F, 0x0C);
			modifyRegister(REG_BTR0, 0x3F, 30);
			break;
		
		case 50000:
			modifyRegister(REG_BTR1, 0x0F, 0x0C);
			modifyRegister(REG_BTR0, 0x3F, 49);
			break;
		
		default:
			return false;
			break;
	}
	
	esp_chip_info_t chip;
	esp_chip_info(&chip);
	if(chip.revision >= 2)
	{
		// From >= rev2 used as "divide BRP by 2"
		modifyRegister(REG_IER, 0x10, 0);
	}
#endif

#ifdef ARDUINO_ESP32S3
	
	// Для 500000
	modifyRegister(REG_BTR0, 0xDFFF, 0xC003);
	modifyRegister(REG_BTR1, 0x00FF, 0x5C);
	modifyRegister(REG_CDR, 0x1FF, 0x0100);

#endif
	
	// Включаем все прерывания
	modifyRegister(REG_IER, 0xEF, 0xEF);

	// Разрешаем любой пакет в фильтрах
	writeRegister(REG_ACRn(0), 0x00);
	writeRegister(REG_ACRn(1), 0x00);
	writeRegister(REG_ACRn(2), 0x00);
	writeRegister(REG_ACRn(3), 0x00);
	writeRegister(REG_AMRn(0), 0xFF);
	writeRegister(REG_AMRn(1), 0xFF);
	writeRegister(REG_AMRn(2), 0xFF);
	writeRegister(REG_AMRn(3), 0xFF);

	// Сброс ошибок и фдагов прерывания
	writeRegister(REG_TXERR, 0x00);
	writeRegister(REG_RXERR, 0x00);
	readRegister(REG_ECC);
	readRegister(REG_IR);

	// Входим в режим Normal + Acceptance Filter Mode
	modifyRegister(REG_MOD, 0x0F, 0x08);
	
	return true;
}

void ESP32SJA1000Class::end()
{
	if(_intrHandle)
	{
		esp_intr_free(_intrHandle);
		_intrHandle = nullptr;
	}

#ifdef ARDUINO_ESP32S3
	SET_PERI_REG_MASK(SYSTEM_PERIP_RST_EN0_REG, SYSTEM_TWAI_RST);
	CLEAR_PERI_REG_MASK(SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_TWAI_CLK_EN);
#else
	DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_CAN_RST);
	DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_CAN_CLK_EN);
#endif
	
	return;
}


bool ESP32SJA1000Class::beginPacket(uint16_t id, bool rtr)
{
	if(id > 0x7FF) return false;
	
	_tx.flag = true;
	_tx.id = id;
	_tx.extended = false;
	_tx.rtr = rtr;
	_tx.length = 0;
	memset(_tx.data, 0x00, sizeof(_tx.data));
	
	return true;
}

bool ESP32SJA1000Class::beginExtendedPacket(uint32_t id, bool rtr)
{
	if(id > 0x1FFFFFFF) return false;
	
	_tx.flag = true;
	_tx.id = id;
	_tx.extended = true;
	_tx.rtr = rtr;
	_tx.length = 0;
	memset(_tx.data, 0x00, sizeof(_tx.data));
	
	return true;
}

uint8_t ESP32SJA1000Class::write(const uint8_t *buffer, uint8_t size)
{
	if(_tx.flag == false) return 0;
	
	if(size > (sizeof(_tx.data) - _tx.length))
	{
		size = sizeof(_tx.data) - _tx.length;
	}
	memcpy(&_tx.data[_tx.length], buffer, size);
	_tx.length += size;
	
	return size;
}

bool ESP32SJA1000Class::endPacket()
{
	if(_tx.flag == false) return false;
	_tx.flag = false;
	
	// wait for TX buffer to free
	while( (readRegister(REG_SR) & 0x04) != 0x04)
	{
		yield();
	}
	
	uint8_t dataReg;
	if(_tx.extended == true)
	{
		writeRegister(REG_EFF, (0x80 | (_tx.rtr ? 0x40 : 0x00) | (0x0F & _tx.length)));
		writeRegister(REG_EFF + 1, _tx.id >> 21);
		writeRegister(REG_EFF + 2, _tx.id >> 13);
		writeRegister(REG_EFF + 3, _tx.id >> 5);
		writeRegister(REG_EFF + 4, _tx.id << 3);
		dataReg = REG_EFF + 5;
	} else {
		writeRegister(REG_SFF, ((_tx.rtr ? 0x40 : 0x00) | (0x0F & _tx.length)));
		writeRegister(REG_SFF + 1, _tx.id >> 3);
		writeRegister(REG_SFF + 2, _tx.id << 5);
		dataReg = REG_SFF + 3;
	}
	
	for(uint8_t i = 0; i < _tx.length; ++i)
	{
		writeRegister(dataReg + i, _tx.data[i]);
	}
	
	if(_loopback == true)
	{
		// self reception request
		modifyRegister(REG_CMR, 0x1F, 0x10);
	} else {
		// transmit request
		modifyRegister(REG_CMR, 0x1F, 0x01);
	}
	
	// wait for TX complete
	while( (readRegister(REG_SR) & 0x08) != 0x08)
	{
		if(readRegister(REG_ECC) == 0xD9)
		{
			// error, abort
			modifyRegister(REG_CMR, 0x1F, 0x02);
			
			return false;
		}
		
		yield();
	}
	
	return true;
}

bool ESP32SJA1000Class::SendPacket(packet_new_t &packet)
{
	(packet.extended == false) ? beginPacket(packet.id) : beginExtendedPacket(packet.id);
	write(packet.data, packet.length);
	return endPacket();
}





bool ESP32SJA1000Class::parsePacket()
{
	if((readRegister(REG_SR) & 0x01) != 0x01) return false;
	
	_rx.extended = (readRegister(REG_SFF) & 0x80) ? true : false;
	_rx.rtr = (readRegister(REG_SFF) & 0x40) ? true : false;
	_rx.dlc = (readRegister(REG_SFF) & 0x0F);
	
	uint8_t dataReg;
	if(_rx.extended == true)
	{
		_rx.id = (readRegister(REG_EFF + 1) << 21) | (readRegister(REG_EFF + 2) << 13) | (readRegister(REG_EFF + 3) << 5) | (readRegister(REG_EFF + 4) >> 3);
		dataReg = REG_EFF + 5;
	} else {
		_rx.id = (readRegister(REG_SFF + 1) << 3) | ((readRegister(REG_SFF + 2) >> 5) & 0x07);
		
		dataReg = REG_SFF + 3;
	}
	
	if(_rx.rtr == true)
	{
		_rx.length = 0;
	} else {
		_rx.length = _rx.dlc;
		for(uint8_t i = 0; i < _rx.length; ++i)
		{
			_rx.data[i] = readRegister(dataReg + i);
		}
	}
	
	// release RX buffer
	modifyRegister(REG_CMR, 0x04, 0x04);
	
	return true;
}





bool ESP32SJA1000Class::filter(uint16_t id, uint16_t mask)
{
	id &= 0x07FF;
	mask &= 0x07FF;
	
	modifyRegister(REG_MOD, 0x17, 0x01);
	
	writeRegister(REG_ACRn(0), ((id >> 3) & 0xFF));
	writeRegister(REG_ACRn(1), ((id << 5) & 0xE0));
	writeRegister(REG_ACRn(2), 0x00);
	writeRegister(REG_ACRn(3), 0x00);
	
	writeRegister(REG_AMRn(0), ((mask >> 3) & 0xFF));
	writeRegister(REG_AMRn(1), ((mask << 5) & 0xE0));
	writeRegister(REG_AMRn(2), 0xFF);
	writeRegister(REG_AMRn(3), 0xFF);
	
	modifyRegister(REG_MOD, 0x17, 0x00);
	
	return true;
}

bool ESP32SJA1000Class::filterExtended(uint32_t id, uint32_t mask)
{
	id &= 0x1FFFFFFF;
	mask &= 0x1FFFFFFF;
	
	modifyRegister(REG_MOD, 0x17, 0x01);
	
	writeRegister(REG_ACRn(0), ((id >> 21) & 0xFF));
	writeRegister(REG_ACRn(1), ((id >> 13) & 0xFF));
	writeRegister(REG_ACRn(2), ((id >> 5) & 0xFF));
	writeRegister(REG_ACRn(3), ((id << 3) & 0xF8));
	
	writeRegister(REG_AMRn(0), ((mask >> 21) & 0xFF));
	writeRegister(REG_AMRn(1), ((mask >> 13) & 0xFF));
	writeRegister(REG_AMRn(2), ((mask >> 5) & 0xFF));
	writeRegister(REG_AMRn(3), ((mask << 3) & 0xF8));
	
	modifyRegister(REG_MOD, 0x17, 0x00);
	
	return true;
}





void ESP32SJA1000Class::cmd_reset()
{
	return;
}

void ESP32SJA1000Class::cmd_observe()
{
	modifyRegister(REG_MOD, 0x17, 0x01);
	modifyRegister(REG_MOD, 0x17, 0x02);
	
	return;
}

void ESP32SJA1000Class::cmd_loopback()
{
	modifyRegister(REG_MOD, 0x17, 0x01);
	modifyRegister(REG_MOD, 0x17, 0x04);
	_loopback = true;
	
	return;
}

void ESP32SJA1000Class::cmd_sleep()
{
	modifyRegister(REG_MOD, 0x1F, 0x10);
	
	return;
}

void ESP32SJA1000Class::cmd_wakeup()
{
	modifyRegister(REG_MOD, 0x1F, 0x00);
	
	return;
}


void ESP32SJA1000Class::onInterrupt(void *arg)
{
	((ESP32SJA1000Class *)arg)->handleInterrupt();
}

void ESP32SJA1000Class::handleInterrupt()
{
	if(readRegister(REG_IR) & 0x01 != 0x01) return;

	if( parsePacket() )
	{
		//packet_new_t rx_cold = _rx;
		_onReceive(_rx);
		memset(&_rx, 0x00, sizeof(_rx));
	}
	
	return;
}


uint32_t ESP32SJA1000Class::readRegister(uint32_t address)
{
	volatile uint32_t *reg = (volatile uint32_t *)(DR_REG_CAN_BASE + (address * 4));
	
	return *reg;
}

void ESP32SJA1000Class::modifyRegister(uint32_t address, uint32_t mask, uint32_t value)
{
	volatile uint32_t *reg = (volatile uint32_t *)(DR_REG_CAN_BASE + (address * 4));
	
	*reg = (*reg & ~mask) | value;
}

void ESP32SJA1000Class::writeRegister(uint32_t address, uint32_t value)
{
	volatile uint32_t *reg = (volatile uint32_t *)(DR_REG_CAN_BASE + (address * 4));
	
	*reg = value;
}

bool ESP32SJA1000Class::writeReadRegister(uint32_t address, uint32_t value)
{
	writeRegister(address, value);
	if(readRegister(address) != value) return false;
	
	return true;
}
