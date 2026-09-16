#pragma once
#include <inttypes.h>
#include "L3Manager.hpp"
#include "Drivers/L3DriverDashboard.hpp"


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



uint32_t MyMillis()
{
	return (esp_timer_get_time() / 1000ULL);
}

uint16_t L3Rx(L3Driver *driver, const uint8_t *rx, uint16_t rx_len)
{
	uint16_t fact_length = 0;

	uint8_t fId = rx[0];
	switch(fId)
	{
		// Ping
		case 0x01:
		{

		}
	}

	return fact_length;
}


L3Manager manager(&L3Rx, &MyMillis);
L3DriverDashboard dash;


structpack can_frame_t
{
	uint8_t fId = 0x17;		// Пакет изменения состояния устройства CAN
	uint16_t canId;			// ID устройства на шине CAN
	uint8_t data_len;		// CAN длина данных
	uint8_t data[8];		// CAN данные
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

	manager.AddPacket(L3DriverInterface::DEVTYPE_DASHBOARD, (uint8_t *)&p1, 6);
	manager.AddPacket(L3DriverInterface::DEVTYPE_DASHBOARD, (uint8_t *)&p2, 6);
	manager.Commit(L3DriverInterface::DEVTYPE_DASHBOARD);



	manager.Processing();
}
