#pragma once
#include <inttypes.h>
#include "DrakeScriptMapping.hpp"
#include "DrakeScriptRegisters.hpp"
#include "DrakeScriptOperators.hpp"

using namespace DrakeScript;

class DrakeScriptCore
{
	static constexpr uint8_t MAX_CUSTOM_OPCODE = 16;
	
	using opcode_func_t = void (*)(DrakeScriptRegisters &registers, const uint8_t *bytes, uint16_t &offset);
	
	public:
		
		DrakeScriptCore(DrakeScriptMapping &map) : _mapping(map), _trigger_data{}, _custom_opcode{}
		{}
		
		void RegCustomOpcode(opcode_idx_t opcode, opcode_func_t func)
		{
			for(auto &obj : _custom_opcode)
			{
				if(obj.opcode != 0x00) continue;
				
				obj = {opcode, func};
				break;
			}
			
			return;
		}
		
		void DelCustomOpcode(opcode_idx_t opcode)
		{
			for(auto &obj : _custom_opcode)
			{
				if(obj.opcode != opcode) continue;
				
				obj = {(opcode_idx_t)0x00, nullptr};
				break;
			}
			
			return;
		}

		/*
			Триггер запуска скрипта
			 - `uint16_t script_id` - ID скрипта, например CAN ID;
			 - `const uint8_t *data` - Данные передаваемые в скрипт для парсинга, например данные CAN;
			 - `uint8_t length` - Длина данных;
		*/
		void Trigger(uint16_t script_id, const uint8_t *data, uint8_t length)
		{
			uint8_t *script_ptr = nullptr;
			uint16_t script_length = 0;
			if(_mapping.GetScriptPtr(script_id, script_ptr, script_length) == false) return;
			
			_trigger_data.script_id = script_id;
			_trigger_data.data = data;
			_trigger_data.length = length;
			
			_RunScript(script_ptr, script_length);
			
			return;
		}
		
	private:
		
		void _RunScript(uint8_t *script_ptr, uint16_t script_length)
		{
			uint8_t *pointer = nullptr;
			uint16_t offset = 0;

			do
			{
				pointer = &script_ptr[offset];
				_RunOpcode(pointer, offset);
			
			} while(offset < script_length);
			
			return;
		}
		
		/*
			Запуск выполнения текущего опкода с параметрами
			 - `const uint8_t *bytes` - указатель на начало опкода с парметрами;
			 - `uint16_t &offset` - смещение для расчёта следующего опкода, байт;
		*/
		void _RunOpcode(const uint8_t *bytes, uint16_t &offset)
		{
			opcode_idx_t opcode = (opcode_idx_t)bytes[0];
			
			switch(opcode)
			{
				case OP_ScriptInit:
				{
					ScriptInit_t *obj = (ScriptInit_t *) bytes;

					_registers.RegisterAllClear();
					
					_registers.RegisterSet(_registers.REG_SCRIPT_ID, _trigger_data.script_id);
					
					if(obj->mode == 0)
					{
						_registers.Register(_registers.REG_PARAM0) = obj->data[0];
						_registers.Register(_registers.REG_PARAM1) = obj->data[1];
						_registers.Register(_registers.REG_PARAM2) = obj->data[2];
						_registers.Register(_registers.REG_PARAM3) = obj->data[3];
					}
					else
					{
						int32_t value = obj->data[0] | (obj->data[1] << 8) | (obj->data[2] << 16) | (obj->data[3] << 24);
						_registers.Register(_registers.REG_PARAM0) = value;
					}
					
					offset += sizeof(*obj);
					break;
				}
				case OP_TriggerParseReg:
				{
					TriggerParseReg_t *obj = (TriggerParseReg_t *) bytes;

					_registers.Register(obj->reg1) = read_i32_fast(&_trigger_data.data[obj->offset], obj->type);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_SetScriptArgVal:
				{
					SetScriptArgVal_t *obj = (SetScriptArgVal_t *) bytes;
					
					uint8_t data[4] = 
					{
						obj->data1, 
						obj->data2, 
						obj->data3, 
						obj->data4 
					};
					_SetScriptArg(obj->script_id, 0, data);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_SetScriptArgReg8:
				{
					SetScriptArgReg8_t *obj = (SetScriptArgReg8_t *) bytes;
					
					uint8_t data[4] = 
					{
						(uint8_t)(_registers.Register(obj->reg1)), 
						(uint8_t)(_registers.Register(obj->reg2)), 
						(uint8_t)(_registers.Register(obj->reg3)), 
						(uint8_t)(_registers.Register(obj->reg4)) 
					};
					_SetScriptArg(obj->script_id, 0, data);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_SetScriptArgReg32:
				{
					SetScriptArgReg32_t *obj = (SetScriptArgReg32_t *) bytes;
					
					uint8_t data[4] = 
					{
						(uint8_t)(_registers.Register(obj->reg1)), 
						(uint8_t)(_registers.Register(obj->reg1) >> 8), 
						(uint8_t)(_registers.Register(obj->reg1) >> 16), 
						(uint8_t)(_registers.Register(obj->reg1) >> 24) 
					};
					_SetScriptArg(obj->script_id, 1, data);
					
					offset += sizeof(*obj);
					break;
				}
				
				case OP_IfRegValEqu:
				{
					IfRegValEqu_t *obj = (IfRegValEqu_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) == obj->value))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegValNeq:
				{
					IfRegValNeq_t *obj = (IfRegValNeq_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) != obj->value))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegValLss:
				{
					IfRegValLss_t *obj = (IfRegValLss_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) < obj->value))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegValLeq:
				{
					IfRegValLeq_t *obj = (IfRegValLeq_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) <= obj->value))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegValGtr:
				{
					IfRegValGtr_t *obj = (IfRegValGtr_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) > obj->value))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegValGeq:
				{
					IfRegValGeq_t *obj = (IfRegValGeq_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) >= obj->value))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegRegEqu:
				{
					IfRegRegEqu_t *obj = (IfRegRegEqu_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) == _registers.Register(obj->reg2)))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegReglNeq:
				{
					IfRegReglNeq_t *obj = (IfRegReglNeq_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) != _registers.Register(obj->reg2)))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegRegLss:
				{
					IfRegRegLss_t *obj = (IfRegRegLss_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) < _registers.Register(obj->reg2)))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegRegLeq:
				{
					IfRegRegLeq_t *obj = (IfRegRegLeq_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) <= _registers.Register(obj->reg2)))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegRegGtr:
				{
					IfRegRegGtr_t *obj = (IfRegRegGtr_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) > _registers.Register(obj->reg2)))
						offset = obj->to_addr;
					
					break;
				}
				case OP_IfRegRegGeq:
				{
					IfRegRegGeq_t *obj = (IfRegRegGeq_t *) bytes;
					
					offset += sizeof(*obj);
					if(!(_registers.Register(obj->reg1) >= _registers.Register(obj->reg2)))
						offset = obj->to_addr;
					
					break;
				}
				case OP_SetRegVal:
				{
					SetRegVal_t *obj = (SetRegVal_t *) bytes;
					
					_registers.Register(obj->reg1) = obj->value;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_SetRegReg:
				{
					SetRegReg_t *obj = (SetRegReg_t *) bytes;
					
					_registers.Register(obj->reg1) = _registers.RegisterGet(obj->reg2);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_IncReg:
				{
					IncReg_t *obj = (IncReg_t *) bytes;

					_registers.Register(obj->reg1) += 1;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_DecReg:
				{
					DecReg_t *obj = (DecReg_t *) bytes;
					
					_registers.Register(obj->reg1) -= 1;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_NotReg:
				{
					NotReg_t *obj = (NotReg_t *) bytes;
					
					_registers.Register(obj->reg1) = ~_registers.Register(obj->reg1);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_ShiftLeftReg:
				{
					ShiftLeftReg_t *obj = (ShiftLeftReg_t *) bytes;
					
					_registers.Register(obj->reg1) <<= obj->count;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_ShiftRightReg:
				{
					ShiftRightReg_t *obj = (ShiftRightReg_t *) bytes;
					
					_registers.Register(obj->reg1) >>= obj->count;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_AndRegVal:
				{
					AndRegVal_t *obj = (AndRegVal_t *) bytes;
					
					_registers.Register(obj->reg1) &= obj->value;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_AndRegReg:
				{
					AndRegReg_t *obj = (AndRegReg_t *) bytes;
					
					_registers.Register(obj->reg1) &= _registers.Register(obj->reg2);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_OrRegVal:
				{
					OrRegVal_t *obj = (OrRegVal_t *) bytes;
					
					_registers.Register(obj->reg1) |= obj->value;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_OrRegReg:
				{
					OrRegReg_t *obj = (OrRegReg_t *) bytes;
					
					_registers.Register(obj->reg1) |= _registers.Register(obj->reg2);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_AddRegVal:
				{
					AddRegVal_t *obj = (AddRegVal_t *) bytes;
					
					_registers.Register(obj->reg1) += obj->value;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_SubRegVal:
				{
					SubRegVal_t *obj = (SubRegVal_t *) bytes;
					
					_registers.Register(obj->reg1) -= obj->value;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_MulRegVal:
				{
					MulRegVal_t *obj = (MulRegVal_t *) bytes;
					
					_registers.Register(obj->reg1) *= obj->value;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_DivRegVal:
				{
					DivRegVal_t *obj = (DivRegVal_t *) bytes;
					
					_registers.Register(obj->reg1) /= obj->value;
					
					offset += sizeof(*obj);
					break;
				}
				case OP_AddRegReg:
				{
					AddRegReg_t *obj = (AddRegReg_t *) bytes;
					
					_registers.Register(obj->reg1) += _registers.Register(obj->reg2);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_SubRegReg:
				{
					SubRegReg_t *obj = (SubRegReg_t *) bytes;
					
					_registers.Register(obj->reg1) -= _registers.Register(obj->reg2);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_MulRegReg:
				{
					MulRegReg_t *obj = (MulRegReg_t *) bytes;
					
					_registers.Register(obj->reg1) *= _registers.Register(obj->reg2);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_DivRegReg:
				{
					DivRegReg_t *obj = (DivRegReg_t *) bytes;
					
					_registers.Register(obj->reg1) /= _registers.Register(obj->reg2);
					
					offset += sizeof(*obj);
					break;
				}
				case OP_Goto:
				{
					Goto_t *obj = (Goto_t *) bytes;
					
					offset = obj->to_addr;
					break;
				}
				case OP_Exit:
				case 0x00:
				case 0xFF:
				{
					Exit_t *obj = (Exit_t *) bytes;
					
					offset = UINT16_MAX;
					break;
				}
				default:
				{

					// Поиск пользовательских опкодов
					uint16_t old_offset = offset;
					for(auto &obj : _custom_opcode)
					{
						if(obj.opcode == opcode)
						{
							obj.func(_registers, bytes, offset);
							break;
						}
					}
					
					if(old_offset == offset)
					{
						offset = UINT16_MAX;
						break;
					}
					
					break;
				}
			}
			
			return;
		}
		
		void _SetScriptArg(uint16_t script_id, uint8_t mode, uint8_t *data)													// Сделать саморедактирование, например по 0xFFFF
		{
			uint8_t *script_ptr = nullptr;
			uint16_t script_length = 0;
			if(_mapping.GetScriptPtr(script_id, script_ptr, script_length) == false) return;
			
			ScriptInit_t *obj = (ScriptInit_t *) script_ptr;
			obj->mode = mode;
			memcpy(obj->data, data, sizeof(obj->data));
			
			return;
		}
		
		DrakeScriptMapping &_mapping;
		DrakeScriptRegisters _registers;
		
		struct trigger_data_t
		{
			uint16_t script_id;
			const uint8_t *data;
			uint8_t length;
		} _trigger_data;
		
		struct custom_opcode_t
		{
			opcode_idx_t opcode;
			opcode_func_t func;
		} _custom_opcode[MAX_CUSTOM_OPCODE];
};
