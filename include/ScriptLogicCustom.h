#pragma once
#include <inttypes.h>

extern L2Wrapper L2;

namespace ScriptLogic
{
	
	struct __attribute__((packed)) CanSendRaw11_t
	{
		uint8_t opcode;
		uint16_t can_id : 12;
		uint16_t length : 4;
		uint8_t data[8];
	};
	
	struct __attribute__((packed)) CanSendRegVal11_t
	{
		uint8_t opcode;
		uint16_t can_id : 12;
		uint16_t length : 4;
		uint8_t fid;
		var_type_t type;
		reg_idx_t reg1;
		reg_idx_t reg2;
		reg_idx_t reg3;
	};
	
	void TestOpcode(DrakeScriptRegisters &registers, const uint8_t *bytes, uint16_t &offset)
	{
		opcode_idx_t opcode = (opcode_idx_t)bytes[0];
		
		switch(opcode)
		{
			case 0xA0:
			{
				CanSendRaw11_t *obj = (CanSendRaw11_t *) bytes;

				L2.Send(obj->can_id, obj->data, obj->length);

				offset += sizeof(*obj);
				break;
			}
			case 0xA1:
			{
				CanSendRegVal11_t *obj = (CanSendRegVal11_t *) bytes;
				
				uint8_t data[8] = {0x00};
				uint8_t offset = 0;
				
				data[offset++] = obj->fid;
				
				if(obj->reg1 != 0xFF)
					offset += write_i32_fast(&data[offset], registers.RegisterGet(obj->reg1), obj->type);
				
				if(obj->reg2 != 0xFF)
					offset += write_i32_fast(&data[offset], registers.RegisterGet(obj->reg2), obj->type);
				
				if(obj->reg3 != 0xFF)
					offset += write_i32_fast(&data[offset], registers.RegisterGet(obj->reg3), obj->type);
				
				L2.Send(obj->can_id, data, obj->length);
				
				offset += sizeof(*obj);
				break;
			}
		}
		
		return;
	}

};
