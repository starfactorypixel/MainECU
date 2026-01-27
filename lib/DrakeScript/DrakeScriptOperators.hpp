#pragma once
#include <inttypes.h>
#include "DrakeScriptRegisters.hpp"

namespace DrakeScript
{
	typedef DrakeScriptRegisters::reg_idx_t reg_idx_t;			// В core берутся эти типа а не из DrakeScriptRegisters.
	typedef DrakeScriptRegisters::reg_type_t reg_type_t;
	
	enum opcode_idx_t : uint8_t
	{
		OP_ScriptInit = 0x01,
		OP_TriggerParseReg = 0x55,
		OP_SetScriptArgVal = 0x02,
		OP_SetScriptArgReg8 = 0x03,
		OP_SetScriptArgReg32 = 0x04,
		OP_IfRegValEqu = 0x05,
		OP_IfRegValNeq = 0x06,
		OP_IfRegValLss = 0x07,
		OP_IfRegValLeq = 0x08,
		OP_IfRegValGtr = 0x09,
		OP_IfRegValGeq = 0x0A,
		OP_IfRegRegEqu = 0x0B,
		OP_IfRegReglNeq = 0x0C,
		OP_IfRegRegLss = 0x0D,
		OP_IfRegRegLeq = 0x0E,
		OP_IfRegRegGtr = 0x0F,
		OP_IfRegRegGeq = 0x10,
		OP_SetRegVal = 0x11,
		OP_SetRegReg = 0x12,
		OP_IncReg = 0x13,
		OP_DecReg = 0x14,
		OP_ShiftLeftReg = 0x19,
		OP_ShiftRightReg = 0x1A,
		OP_AddRegVal = 0x15,
		OP_SubRegVal = 0x16,
		OP_MulRegVal = 0x17,
		OP_DivRegVal = 0x18,
		OP_AddRegReg = 0x1F,
		OP_SubRegReg = 0x20,
		OP_MulRegReg = 0x21,
		OP_DivRegReg = 0x22,
		//OP_CanSendRaw11 = 0x1B,
		//OP_CanSendRegVal11 = 0x1C,
		OP_Goto = 0x1D,
		OP_Exit = 0x1E,
	};

	enum var_type_t : uint8_t
	{
		VAR_BYTE = 0,
		VAR_U8 = 1,
		VAR_S8 = 2,
		VAR_U16 = 3,
		VAR_S16 = 4,
		VAR_U32 = 5,
		VAR_S32 = 6,
	};
	
	
	struct __attribute__((packed)) ScriptInit_t
	{
		uint8_t opcode;
		uint8_t mode;
		uint8_t data[4];
	};
	
	struct __attribute__((packed)) TriggerParseReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		var_type_t type;
		uint8_t offset;
	};
	
	struct __attribute__((packed)) SetScriptArgVal_t 
	{
		uint8_t opcode;
		uint16_t script_id;
		uint8_t data1;
		uint8_t data2;
		uint8_t data3;
		uint8_t data4;
	};

	struct __attribute__((packed)) SetScriptArgReg8_t
	{
		uint8_t opcode;
		uint16_t script_id;
		reg_idx_t reg1;
		reg_idx_t reg2;
		reg_idx_t reg3;
		reg_idx_t reg4;
	};
	
	struct __attribute__((packed)) SetScriptArgReg32_t
	{
		uint8_t opcode;
		uint16_t script_id;
		reg_idx_t reg1;
	};



	struct __attribute__((packed)) IfRegValEqu_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegValNeq_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegValLss_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegValLeq_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegValGtr_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegValGeq_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
		uint16_t offset;
	};

	struct __attribute__((packed)) IfRegRegEqu_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegReglNeq_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegRegLss_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegRegLeq_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegRegGtr_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
		uint16_t offset;
	};
	struct __attribute__((packed)) IfRegRegGeq_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
		uint16_t offset;
	};


	struct __attribute__((packed)) SetRegVal_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
	};
	struct __attribute__((packed)) SetRegReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
	};
	struct __attribute__((packed)) IncReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
	};
	struct __attribute__((packed)) DecReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
	};

	struct __attribute__((packed)) ShiftLeftReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		uint8_t count;
	};
	struct __attribute__((packed)) ShiftRightReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		uint8_t count;
	};

	struct __attribute__((packed)) AddRegVal_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
	};
	struct __attribute__((packed)) SubRegVal_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
	};
	struct __attribute__((packed)) MulRegVal_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
	};
	struct __attribute__((packed)) DivRegVal_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_type_t value;
	};

	struct __attribute__((packed)) AddRegReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
	};
	struct __attribute__((packed)) SubRegReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
	};
	struct __attribute__((packed)) MulRegReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
	};
	struct __attribute__((packed)) DivRegReg_t
	{
		uint8_t opcode;
		reg_idx_t reg1;
		reg_idx_t reg2;
	};
	
	struct __attribute__((packed)) Goto_t
	{
		uint8_t opcode;
		uint16_t offset;
	};
	struct __attribute__((packed)) Exit_t
	{
		uint8_t opcode;
	};















	static inline reg_type_t read_i32_fast(const uint8_t *data, var_type_t type)
	{
		switch(type)
		{
			case VAR_BYTE:
			case VAR_U8:
			{
				uint8_t v;
				memcpy(&v, data, sizeof(v));
				return v;
			}
			case VAR_S8:
			{
				int8_t v;
				memcpy(&v, data, sizeof(v));
				return v;
			}
			case VAR_U16:
			{
				uint16_t v;
				memcpy(&v, data, sizeof(v));
				return v;
			}
			case VAR_S16:
			{
				int16_t v;
				memcpy(&v, data, sizeof(v));
				return v;
			}
			case VAR_U32:
			{
				uint32_t v;
				memcpy(&v, data, sizeof(v));
				return (reg_type_t)v;
			}
			case VAR_S32:
			{
				int32_t v;
				memcpy(&v, data, sizeof(v));
				return v;
			}
		}
		
		return 0;
	}



	static uint8_t write_i32_fast(uint8_t* dst, reg_type_t val, var_type_t type)
	{
		switch (type)
		{
			case VAR_BYTE:
			case VAR_U8:
				*(uint8_t*)dst = (uint8_t)val;
				return 1;

			case VAR_S8:
				*(int8_t*)dst = (int8_t)val;
				return 1;

			case VAR_U16:
				*(uint16_t*)dst = (uint16_t)val;
				return 2;

			case VAR_S16:
				*(int16_t*)dst = (int16_t)val;
				return 2;

			case VAR_U32:
				*(uint32_t*)dst = (uint32_t)val;
				return 4;

			case VAR_S32:
				*(int32_t*)dst = (int32_t)val;
				return 4;

			default:
				return 0;
		}
	}

	
};
