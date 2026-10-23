#pragma once
#include <inttypes.h>
#include "Drivers/L3DriverInterface.hpp"
#include "Packets/L3PacketConsts.hpp"

class L3Driver;

class L3ManagerInterface
{
	public:

		// Сообщение приложения. Вернуть фактическую длину сообщения по fId, 0 — остаток DATA не разбирать
		virtual uint16_t RxPacket(L3Driver *driver, const uint8_t *rx, uint16_t rx_len) = 0;

		// Сеанс открыт: первый валидный пакет клиента
		virtual void SessionOpen(L3Driver *driver) = 0;

		// Сеанс закрыт
		virtual void SessionClose(L3Driver *driver, L3DriverInterface::close_reason_t reason) = 0;

		// Ошибка канала: отклонённый кадр, ошибка отправки, ошибка запуска счётчиков
		virtual void Error(L3Driver *driver, L3PacketConsts::error_t error) = 0;
};
