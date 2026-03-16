#pragma once
#include <inttypes.h>
#include <string.h>

class DrakeScriptRegisters
{
	public:
		
		typedef int32_t reg_type_t;
		enum reg_idx_t : uint8_t
		{
			REG_DUMMY = 0,

			REG_SCRIPT_ID = 1,
			REG_PARAM0 = 2,
			REG_PARAM1 = 3,
			REG_PARAM2 = 4,
			REG_PARAM3 = 5,

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
		
		DrakeScriptRegisters() : _registers{}
		{}
		
		inline void RegisterAllClear()
		{
			memset(_registers, 0x00, sizeof(_registers));
			
			return;
		}
		
		inline reg_type_t RegisterGet(reg_idx_t idx)
		{
			if(idx >= (sizeof(_registers) / sizeof(reg_type_t)))
				return _registers[0];
			
			return _registers[idx];
		}
		
		inline void RegisterSet(reg_idx_t idx, reg_type_t value)
		{
			if(idx >= (sizeof(_registers) / sizeof(reg_type_t)))
				return;
			
			_registers[idx] = value;
			return;
		}
		
		inline reg_type_t &Register(reg_idx_t idx)
		{
			if(idx >= (sizeof(_registers) / sizeof(reg_type_t)))
				return _registers[0];
			
			return _registers[idx];
		}
		
	private:
		
		reg_type_t _registers[1 + 5 + 9];
};
