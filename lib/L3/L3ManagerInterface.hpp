#pragma once
#include <inttypes.h>
#include "Drivers/L3DriverInterface.hpp"
#include "Packets/L3PacketConsts.hpp"

class L3Driver;

class L3ManagerInterface
{
	public:

		// Сообщение приложения: msg[0] — fId, length — длина сообщения (fId + данные)
		virtual void RxPacket(L3Driver *driver, const uint8_t *msg, uint16_t length) = 0;

		// Сеанс открыт: клиент прислал FID_ACCEPT
		virtual void SessionOpen(L3Driver *driver) = 0;

		// Сеанс закрыт
		virtual void SessionClose(L3Driver *driver, L3DriverInterface::close_reason_t reason) = 0;

		// Ошибка канала: отклонённый кадр, ошибка отправки, ошибка запуска счётчиков
		virtual void Error(L3Driver *driver, L3PacketConsts::error_t error) = 0;
};
