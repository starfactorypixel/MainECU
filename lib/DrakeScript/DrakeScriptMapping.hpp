#pragma once
#include <inttypes.h>
#include <string.h>

class DrakeScriptMapping
{
	static constexpr uint16_t MAX_SCRIPTS_COUNT = 2048;
	
	public:
		
		DrakeScriptMapping()
		{
			memset(_scripts_map, 0x00, sizeof(_scripts_map));

			return;
		}

		// Установить указатель на глобальный массив данным со скриптами.
		void SetScriptsArray(uint8_t *array, uint32_t length)
		{
			_scripts_array = array;
			_scripts_array_length = length;

			return;
		}
		
		// Добавить в карту скриптов новый скрипт
		void AddScriptMap(uint16_t id, uint32_t start_idx, uint16_t length)
		{
			if(id >= MAX_SCRIPTS_COUNT) return;
			
			_scripts_map[id] = {start_idx, length, 1};

			return;
		}
		
		
		bool GetScriptMap(uint16_t id, uint8_t *array_ptr, uint16_t &length)
		{
			if(id >= MAX_SCRIPTS_COUNT) return false;
			
			auto &obj = _scripts_map[id];
			array_ptr = &_scripts_array[obj.start_idx];
			length = obj.length;
			
			return true;
		}
		
		bool CheckScriptRunnable(uint16_t id)
		{
			if(id >= MAX_SCRIPTS_COUNT) return false;
			auto &obj = _scripts_map[id];
			if(obj.mode <= 0) return false;
			
			return true;
		}

	private:
		
		struct script_map_t
		{
			uint32_t start_idx;			// Индекс начала скрипта
			uint16_t length;			// Длина скрипта
			int8_t mode;				// Режим работы скрипта: 0 - нету, -1 - отключён пользователем, -2 - отключён по ошибке, 1 - активен.
			//uint8_t _aligning;		// 
		} _scripts_map[MAX_SCRIPTS_COUNT];
		
		uint8_t *_scripts_array = nullptr;
		uint32_t _scripts_array_length = 0;
};
