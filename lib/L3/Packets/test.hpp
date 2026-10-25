#pragma once
#include <inttypes.h>
#include "L3Manager.hpp"
#include "Drivers/L3DriverDashboard.hpp"

/*
TODO
	1. Барьеры памяти в очереди payload (L3PacketPayload поверх CUtils RingBuffer).
	   RingBuffer защищает индексы только volatile. На двухъядерном ESP32-S3 задача приёма (пишет)
	   и runtime (читает) могут работать на разных ядрах, а компилятор вправе переставить запись данных
	   слота после сдвига _head. Тогда runtime прочитает слот до того, как он заполнен.
	   Исправление в L3PacketPayload, без правки CUtils:
		- __atomic_thread_fence(__ATOMIC_RELEASE) — перед сдвигом _head (после memcpy данных)
		  и перед сдвигом _tail (после чтения данных);
		- __atomic_thread_fence(__ATOMIC_ACQUIRE) — после проверок IsEmpty() / IsFull().
	   L3PacketRx::Reset() (Clear очереди) — только при остановленной задаче приёма.

СДЕЛАНО
	- Привязка пакетов клиента к сеансу ESP: клиент дописывает в конец AAD 4 байта идентификатора сеанса
	  (IV beacon [8..11]), в эфир они не идут. Идентификатор новый при запуске, при закрытии сеанса и на
	  каждом beacon, пока сеанса нет. Защищает от повтора пакетов прошлого запуска (эпоха не успела
	  сохраниться, Config пишет раз в 30 с) и от недоставленных пакетов закрытого сеанса.
*/


/*
Приём пакетов:
	0. (ISR) Приём пакетов до IDLE. Драйвер должен реализовать этот IDLE или средствами HW или самостоятельно.
	   Формирует по одному пакету.
	1. (RtOS) Полученный пакет передаётся в transport для анализа и проверки. Если целый то дальше, иначе - отбрасываем (событие ошибки L3RxTransportError + код).
	2. (RtOS) Полученный payload от transport передаётся в security для анализа и проверки. Если целый то дальше, иначе - отбрасываем (событие ошибки L3RxSecurityError + код).
	3. (RtOS) Полученный payload от security сохраняем в кольцевой буфер RX пакетов.
	4. (Runtime) Вычитываем весь кольцевой буфер RX пакетов, разбираем его на отдельные пакеты и вызываем колбэки.
	5. 

Передача пакетов:
	1. (Runtime) Код в любом месте генерирует новый пакет и добавляет его в payload пакет.
	   Если влезает, то просто добавляем, если нет, то выполняем дальше а затем добавляем в чистый payload пакет.
	2. (Runtime) Копируем payload пакет в security payload пакет.
	3. (Runtime) Копируем security пакет в transport payload пакет.
	4. (Runtime) В transport пакете у нас готовый байт массив для отправки.
	5. (Runtime) Пытаемся отправить в HW пакет, если не получается то складываем в буфер TX пакетов.
	6. (RtOS) Переодически проверяем возможность отправки пакетов из буфера и отправляем.
	7. 
*/


/*
	1. При первом запуске:
	1.1 Генерирует секретный ключ (32 байта);
	1.2 Генерируем серийный номер устройства (8 байт);

	2. Общение планшета:
	2.1 При подключении планшет пытается проверить свой KEY отправив команду (check_private_key_t) подписав её последним использованным KEY.
	    Если KEY не подошёл, esp отвечает в режиме raw код ошибки и свой SN.
		   Планшет использует SN для поиска KEY в базе.
		   Если SN не найден показываем форму добавления KEY.
		Если KEY подошёл, esp отвечает в режиме raw код успеха и свой SN.
	2.1 


	Алгоритм beacon.
	1. Каждый N секунд в эфир \ кабель \ сеть отправляем свой SN
	   Этот же пакет является пакетом ping в режиме связи.


*/



uint32_t MyMillis()
{
	return (esp_timer_get_time() / 1000ULL);
}

void L3Rx(L3Driver *driver, const uint8_t *msg, uint16_t length)
{
	uint8_t fId = msg[0];
	switch(fId)
	{
		default:
		{
			break;
		}
	}
}

void L3Open(L3Driver *driver)
{
	DEBUG_LOG_TOPIC("L3", "Session open, dev 0x%02X\n", driver->GetType());
}

void L3Close(L3Driver *driver, L3DriverInterface::close_reason_t reason)
{
	DEBUG_LOG_TOPIC("L3", "Session close, dev 0x%02X, reason %u\n", driver->GetType(), reason);
}

void L3Error(L3Driver *driver, L3C::error_t error)
{
	DEBUG_LOG_TOPIC("L3", "Error, dev 0x%02X: %s\n", driver->GetType(), L3C::ErrorName(error));
}

const L3Manager::callbacks_t l3_callbacks = {&L3Open, &L3Close, &L3Rx, &L3Error};
L3Manager manager(l3_callbacks, &MyMillis);
L3DriverDashboard dash;


structpack can_frame_t
{
	uint8_t fId = 0x17;		// Пакет изменения состояния устройства CAN
	uint16_t canId;			// ID устройства на шине CAN
	uint8_t data_len;		// CAN длина данных
	uint8_t data[8];		// CAN данные

	uint8_t GetLen(){ return data_len + 4; }
};


void qwe()
{
	can_frame_t p1 = {};
	p1.canId = 0x0126;
	p1.data_len = 2;
	p1.data[0] = 0x65;
	p1.data[1] = 0xFF;
	
	can_frame_t p2 = {};
	p2.canId = 0x0127;
	p2.data_len = 2;
	p2.data[0] = 0x65;
	p2.data[1] = 0x00;

	manager.AddDriver(dash);

	sizeof(can_frame_t);

	L3DriverInterface::dev_type_t type = dash.GetType();

	manager.AddPacket(type, (uint8_t *)&p1, p1.GetLen());
	manager.AddPacket(type, (uint8_t *)&p2, p2.GetLen());
	manager.Commit(type);



	manager.Processing();
}
