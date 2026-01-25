#pragma once
#include <inttypes.h>
#include <string.h>

namespace DrakeScript
{
	static constexpr uint8_t USER_REGISTER_COUNT = 5;
	
	typedef uint32_t register_type_t;
	enum reg_idx_t : uint8_t
	{
		REG_DUMMY = 0,

		REG_PARAM1 = 1,
		REG_PARAM2 = 2,
		REG_PARAM3 = 3,
		REG_PARAM4 = 4,

		REG_CANRX_ID = 5,
		REG_CANRX_FID = 6,
		REG_CANRX_VALUE1 = 7,
		REG_CANRX_VALUE2 = 8,
		REG_CANRX_VALUE3 = 9,

		REG_A = 10,
		REG_B = 11,
		REG_C = 12,
		REG_D = 13,
		REG_E = 14,
	};

	register_type_t registers[4 + 5 + USER_REGISTER_COUNT];


	void inline RegisterAllClear()
	{
		memset(registers, 0x00, sizeof(registers));
		
		return;
	}
	
	register_type_t inline RegisterGet(reg_idx_t idx)
	{
		return registers[idx];
	}
	
	void inline RegisterSet(reg_idx_t idx, register_type_t value)
	{
		registers[idx] = value;

		return;
	}
	
	inline register_type_t &Register(reg_idx_t idx)
	{
		return registers[idx];
	}


};
