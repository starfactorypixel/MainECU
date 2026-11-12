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

// Флаги ошибок и событий шины, битовая маска
enum : uint32_t
{
	L2_ERR_NONE					= 0,
	L2_ERR_TX_REJECT			= (1UL << 0),	// Драйвер не принял кадр на отправку (очередь полна или шина недоступна)
	L2_ERR_TX_FAILED			= (1UL << 1),	// Отправка кадра завершилась ошибкой
	L2_ERR_RX_QUEUE_OVERFLOW	= (1UL << 2),	// Потеря при приёме: очередь драйвера полна (loop не успевает)
	L2_ERR_RX_FIFO_OVERRUN		= (1UL << 3),	// Потеря при приёме: переполнен аппаратный FIFO (ISR не успел)
	L2_ERR_BUS_ERROR			= (1UL << 4),	// Ошибка на шине (bit, stuff, form, CRC, ACK)
	L2_ERR_WARNING				= (1UL << 5),	// TEC или REC превысил порог предупреждения (96)
	L2_ERR_PASSIVE				= (1UL << 6),	// Переход в error-passive
	L2_ERR_BUS_OFF				= (1UL << 7),	// Переход в bus-off, запущено восстановление
	L2_EVT_BUS_RECOVERED		= (1UL << 8),	// Не ошибка: восстановление после bus-off завершено, шина снова работает
};


// Статистика L2
// *_count — накопительные счётчики с момента запуска. Остальное — текущие значения на момент запроса
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
