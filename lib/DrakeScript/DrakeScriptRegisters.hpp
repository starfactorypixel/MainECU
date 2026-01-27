#pragma once
#include <inttypes.h>
#include <string.h>

// Добавить проверку на то, что пытаемся получить регистрр, которого нету (за пределами массива)

class DrakeScriptRegisters
{
	static constexpr uint8_t USER_REGISTER_COUNT = 5;
	
	public:
		
		typedef int32_t reg_type_t;
		enum reg_idx_t : uint8_t
		{
			REG_DUMMY = 0,

			REG_PARAM0 = 1,
			REG_PARAM1 = 2,
			REG_PARAM2 = 3,
			REG_PARAM3 = 4,


			REG_SCRIPT_ID = 5,

			REG_A = 6,
			REG_B = 7,
			REG_C = 8,
			REG_D = 9,
			REG_E = 10,
			REG_F = 11,
			REG_G = 12,
			REG_H = 13,
			REG_I = 14,
		};
		
		
		inline void RegisterAllClear()
		{
			memset(registers, 0x00, sizeof(registers));
			
			return;
		}
		
		inline reg_type_t RegisterGet(reg_idx_t idx)
		{
			return registers[idx];
		}
		
		inline void RegisterSet(reg_idx_t idx, reg_type_t value)
		{
			registers[idx] = value;

			return;
		}
		
		inline reg_type_t &Register(reg_idx_t idx)
		{
			return registers[idx];
		}
		
	private:
		
		reg_type_t registers[1 + 4 + 5 + USER_REGISTER_COUNT];
};
