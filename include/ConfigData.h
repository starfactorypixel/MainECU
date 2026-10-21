#pragma once
#include <inttypes.h>
#include <CUtils.h>

namespace Config
{
	structpack config_body_t
	{
		structpack
		{
			bool init = false;			// Флаг инициализированный данных
			uint8_t sn[8];				// Серийный номер
			
			char name[33];				// Имя

			structpack
			{
				uint32_t epoch;			// Эпоха генерации IV. Каждый запуск Main и каждое переполнение counter она ++. Значение ограничено 24 битами.
				uint8_t key[32];		// Приватный ключ
			} dev_01;
		} security;

	};
};
