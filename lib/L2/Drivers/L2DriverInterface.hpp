#pragma once
#include <inttypes.h>
#include "L2Consts.hpp"

class L2DriverInterface
{
	public:
		virtual ~L2DriverInterface() = default;

		virtual bool Begin() = 0;
		virtual void End() = 0;

		// Забрать один принятый кадр. false — принятых кадров нет.
		virtual bool Receive(L2Frame &frame) = 0;

		// Поставить кадр в очередь на отправку. false — очередь полна или шина недоступна.
		virtual bool Transmit(const L2Frame &frame) = 0;

		// Обслуживание драйвера (события шины, восстановление после ошибок). Вызывается из L2Manager::Tick().
		virtual void Poll() = 0;

		// Драйвер запущен и шина в рабочем состоянии.
		virtual bool IsRunning() const = 0;
		
		// Заполнить поля статистики из блоков «Ведёт драйвер» (см. L2Statistics).
		// Поля блока «Ведёт L2Manager» не трогать.
		virtual void GetStatistics(L2Statistics &stats) const = 0;
};
