#pragma once
#include <inttypes.h>

// Кадр CAN, общий для менеджера и всех драйверов
struct L2Frame
{
	uint32_t id;
	uint8_t length;			// 0..8
	uint8_t data[8];
	bool extended;			// 29-битный ID
	bool rtr;				// remote frame
};

// Статистика L2
struct L2Statistics
{
	uint32_t rx_count;				// Кол-во принятых пакетов
	uint32_t tx_count;				// Кол-во переданных пакетов
	uint32_t rx_drop_count;			// Кол-во потерянных пакетов при приёме
	uint32_t tx_drop_count;			// Кол-во потерянных пакетов при отправке
	uint32_t bus_off_count;			// Кол-во отключений от шины
};
