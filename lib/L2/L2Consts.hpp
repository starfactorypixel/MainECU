#pragma once
#include <inttypes.h>

struct L2Frame
{
	uint32_t id;
	uint8_t length;
	uint8_t data[8];
	bool extended;
	bool rtr;
};

// Состояние узла на шине
enum L2BusState : uint8_t
{
	L2_BUS_STOPPED = 0,					// Драйвер не запущен
	L2_BUS_ACTIVE,						// Норма: error-active (TEC и REC < 128)
	L2_BUS_PASSIVE,						// Error-passive: TEC или REC >= 128, узел работает, но не может активно сигнализировать ошибки
	L2_BUS_OFF,							// Bus-off: TEC >= 256, узел отключён от шины
	L2_BUS_RECOVERING,					// Идёт восстановление после bus-off
};

// Статистика L2.
// *_count — накопительные счётчики с момента запуска. Остальное — текущие значения на момент запроса.
struct L2Statistics
{
	// --- Ведёт L2Manager ---
	uint32_t rx_count;					// Принято кадров (передано в колбек)
	uint32_t tx_count;					// Кадров поставлено в очередь отправки
	uint32_t tx_reject_count;			// Кадров не поставлено в очередь (очередь полна или шина недоступна)

	// --- Ведёт драйвер: накопительные ---
	uint32_t rx_queue_overflow_count;	// Потеряно при приёме: программная очередь драйвера полна (loop не успевает)
	uint32_t rx_fifo_overrun_count;		// Потеряно при приёме: переполнен аппаратный FIFO (ISR не успел)
	uint32_t tx_failed_count;			// Кадров, отправка которых завершилась ошибкой
	uint32_t arb_lost_count;			// Проигрышей арбитража
	uint32_t bus_error_count;			// Ошибок на шине
	uint32_t bus_off_count;				// Переходов в bus-off

	// --- Ведёт драйвер: текущие ---
	L2BusState state;					// Состояние узла на шине
	uint16_t rx_pending;				// Кадров в очереди приёма, ещё не прочитано
	uint16_t tx_pending;				// Кадров в очереди отправки, ещё не отправлено
	uint16_t tx_error_counter;			// TEC
	uint16_t rx_error_counter;			// REC
};
