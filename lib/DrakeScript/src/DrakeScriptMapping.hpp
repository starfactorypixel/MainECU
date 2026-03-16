#pragma once
#include <inttypes.h>

class DrakeScriptMapping
{
	static constexpr uint16_t _max_scripts_count = 2048;
	
	public:
		
		DrakeScriptMapping() : _scripts_map{}
		{}
		
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
			if(id >= _max_scripts_count) return;
			
			_scripts_map[id] = {start_idx, length, 1};

			return;
		}
		
		// Получить указатель на начало скрипта и его длину в байтах
		bool GetScriptPtr(uint16_t id, uint8_t *&array_ptr, uint16_t &length)
		{
			if(id >= _max_scripts_count) return false;
			auto &obj = _scripts_map[id];
			if(obj.mode <= 0) return false;
			
			array_ptr = &_scripts_array[obj.start_idx];
			length = obj.length;
			
			return true;
		}
		
	private:
		
		struct script_map_t
		{
			uint32_t start_idx;			// Индекс начала скрипта
			uint16_t length;			// Длина скрипта
			int8_t mode;				// Режим работы скрипта: 0 - нету, -1 - отключён пользователем, -2 - отключён по ошибке, 1 - активен.
			//uint8_t _aligning;		// 
		} _scripts_map[_max_scripts_count];
		
		uint8_t *_scripts_array = nullptr;
		uint32_t _scripts_array_length = 0;
};
