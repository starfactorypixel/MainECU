#pragma once
#include <inttypes.h>
#include "DrakeScriptRegisters.hpp"
#include "DrakeScriptOperators.hpp"

using namespace DrakeScript;

class DrakeScriptCore
{
	
	using custom_opcode_func_t = void (*)(const opcode_idx_t opcode, const uint8_t *bytes, uint16_t &offset);
	


	public:
		DrakeScriptCore(/* args */)
		{
			memset(_custom_opcode, 0x00, sizeof(_custom_opcode));

			return;
		}

		~DrakeScriptCore();

		void hgjghjgh()
		{
			RegisterAllClear();
			RegisterSet(REG_A, 12);
		}


		void RegCustomOpcode(opcode_idx_t opcode, custom_opcode_func_t func)
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



		/// @brief Запуск выполнения текущего опкода с параметрами
		/// @param bytes указатель на начало опкода с парметрами
		/// @param offset смещение для расчёта следуюзего опкода, байт
		void RunOpcode(const uint8_t *bytes, uint16_t &offset)
		{
			opcode_idx_t opcode = (opcode_idx_t)bytes[0];
			
			switch(opcode)
			{
				case OP_ParserCfg:
				{
					ParserCfg_t *obj = (ParserCfg_t *) bytes;

					offset += sizeof(ParserCfg_t);
					break;
				}
				case OP_SetScriptArgVal:
				{
					SetScriptArgVal_t *obj = (SetScriptArgVal_t *) bytes;
					
					uint8_t data[4] = {obj->data1, obj->data2, obj->data3, obj->data4};
					SetScriptArg(obj->script_id, data);
					
					offset += sizeof(SetScriptArgVal_t);
					break;
				}
				case OP_SetScriptArgReg8:
				{
					SetScriptArgReg8_t *obj = (SetScriptArgReg8_t *) bytes;
					
					uint8_t data[4] = {Register(obj->reg1), Register(obj->reg2), Register(obj->reg3), Register(obj->reg4)};
					SetScriptArg(obj->script_id, data);
					
					offset += sizeof(SetScriptArgReg8_t);
					break;
				}
				case OP_SetScriptArgReg32:
				{
					SetScriptArgReg32_t *obj = (SetScriptArgReg32_t *) bytes;
					
					uint8_t data[4] = {Register(obj->reg1), Register(obj->reg1) >> 8, Register(obj->reg1) >> 16, Register(obj->reg1) >> 24};
					SetScriptArg(obj->script_id, data);
					
					offset += sizeof(SetScriptArgReg32_t);
					break;
				}
				
				case OP_IfRegValEqu:
				{
					IfRegValEqu_t *obj = (IfRegValEqu_t *) bytes;
					
					if(!(Register(obj->reg1) == obj->value))
						offset += obj->offset;
					
					offset += sizeof(IfRegValEqu_t);
					break;
				}
				case OP_IfRegValNeq:
				{
					IfRegValNeq_t *obj = (IfRegValNeq_t *) bytes;
					
					if(!(Register(obj->reg1) != obj->value))
						offset += obj->offset;
					
					offset += sizeof(IfRegValNeq_t);
					break;
				}
				case OP_IfRegValLss:
				{
					IfRegValLss_t *obj = (IfRegValLss_t *) bytes;
					
					if(!(Register(obj->reg1) < obj->value))
						offset += obj->offset;
					
					offset += sizeof(IfRegValLss_t);
					break;
				}
				case OP_IfRegValLeq:
				{
					IfRegValLeq_t *obj = (IfRegValLeq_t *) bytes;
					
					if(!(Register(obj->reg1) <= obj->value))
						offset += obj->offset;
					
					offset += sizeof(IfRegValLeq_t);
					break;
				}
				case OP_IfRegValGtr:
				{
					IfRegValGtr_t *obj = (IfRegValGtr_t *) bytes;
					
					if(!(Register(obj->reg1) > obj->value))
						offset += obj->offset;
					
					offset += sizeof(IfRegValGtr_t);
					break;
				}
				case OP_IfRegValGeq:
				{
					IfRegValGeq_t *obj = (IfRegValGeq_t *) bytes;
					
					if(!(Register(obj->reg1) >= obj->value))
						offset += obj->offset;
					
					offset += sizeof(IfRegValGeq_t);
					break;
				}
				case OP_IfRegRegEqu:
				{
					IfRegRegEqu_t *obj = (IfRegRegEqu_t *) bytes;
					
					if(!(Register(obj->reg1) == Register(obj->reg2)))
						offset += obj->offset;
					
					offset += sizeof(IfRegRegEqu_t);
					break;
				}
				case OP_IfRegReglNeq:
				{
					IfRegReglNeq_t *obj = (IfRegReglNeq_t *) bytes;
					
					if(!(Register(obj->reg1) != Register(obj->reg2)))
						offset += obj->offset;
					
					offset += sizeof(IfRegReglNeq_t);
					break;
				}
				case OP_IfRegRegLss:
				{
					IfRegRegLss_t *obj = (IfRegRegLss_t *) bytes;
					
					if(!(Register(obj->reg1) < Register(obj->reg2)))
						offset += obj->offset;
					
					offset += sizeof(IfRegRegLss_t);
					break;
				}
				case OP_IfRegRegLeq:
				{
					IfRegRegLeq_t *obj = (IfRegRegLeq_t *) bytes;
					
					if(!(Register(obj->reg1) <= Register(obj->reg2)))
						offset += obj->offset;
					
					offset += sizeof(IfRegRegLeq_t);
					break;
				}
				case OP_IfRegRegGtr:
				{
					IfRegRegGtr_t *obj = (IfRegRegGtr_t *) bytes;
					
					if(!(Register(obj->reg1) > Register(obj->reg2)))
						offset += obj->offset;
					
					offset += sizeof(IfRegRegGtr_t);
					break;
				}
				case OP_IfRegRegGeq:
				{
					IfRegRegGeq_t *obj = (IfRegRegGeq_t *) bytes;
					
					if(!(Register(obj->reg1) >= Register(obj->reg2)))
						offset += obj->offset;
					
					offset += sizeof(IfRegRegGeq_t);
					break;
				}
				case OP_SetRegVal:
				{
					SetRegVal_t *obj = (SetRegVal_t *) bytes;
					
					Register(obj->reg1) = obj->value;
					
					offset += sizeof(SetRegVal_t);
					break;
				}
				case OP_SetRegReg:
				{
					SetRegReg_t *obj = (SetRegReg_t *) bytes;
					
					Register(obj->reg1) = RegisterGet(obj->reg2);
					
					offset += sizeof(SetRegReg_t);
					break;
				}
				case OP_IncReg:
				{
					IncReg_t *obj = (IncReg_t *) bytes;

					Register(obj->reg1) += 1;
					
					offset += sizeof(IncReg_t);
					break;
				}
				case OP_DecReg:
				{
					DecReg_t *obj = (DecReg_t *) bytes;
					
					Register(obj->reg1) -= 1;
					
					offset += sizeof(DecReg_t);
					break;
				}
				case OP_ShiftLeftReg:
				{
					ShiftLeftReg_t *obj = (ShiftLeftReg_t *) bytes;
					
					Register(obj->reg1) <<= obj->count;
					
					offset += sizeof(ShiftLeftReg_t);
					break;
				}
				case OP_ShiftRightReg:
				{
					ShiftRightReg_t *obj = (ShiftRightReg_t *) bytes;
					
					Register(obj->reg1) >>= obj->count;
					
					offset += sizeof(ShiftRightReg_t);
					break;
				}
				case OP_AddRegVal:
				{
					AddRegVal_t *obj = (AddRegVal_t *) bytes;
					
					Register(obj->reg1) += obj->value;
					
					offset += sizeof(AddRegVal_t);
					break;
				}
				case OP_SubRegVal:
				{
					SubRegVal_t *obj = (SubRegVal_t *) bytes;
					
					Register(obj->reg1) -= obj->value;
					
					offset += sizeof(SubRegVal_t);
					break;
				}
				case OP_MulRegVal:
				{
					MulRegVal_t *obj = (MulRegVal_t *) bytes;
					
					Register(obj->reg1) *= obj->value;
					
					offset += sizeof(MulRegVal_t);
					break;
				}
				case OP_DivRegVal:
				{
					DivRegVal_t *obj = (DivRegVal_t *) bytes;
					
					Register(obj->reg1) /= obj->value;
					
					offset += sizeof(DivRegVal_t);
					break;
				}
				case OP_AddRegReg:
				{
					AddRegReg_t *obj = (AddRegReg_t *) bytes;
					
					Register(obj->reg1) += Register(obj->reg2);
					
					offset += sizeof(AddRegReg_t);
					break;
				}
				case OP_SubRegReg:
				{
					SubRegReg_t *obj = (SubRegReg_t *) bytes;
					
					Register(obj->reg1) -= Register(obj->reg2);
					
					offset += sizeof(SubRegReg_t);
					break;
				}
				case OP_MulRegReg:
				{
					MulRegReg_t *obj = (MulRegReg_t *) bytes;
					
					Register(obj->reg1) *= Register(obj->reg2);
					
					offset += sizeof(MulRegReg_t);
					break;
				}
				case OP_DivRegReg:
				{
					DivRegReg_t *obj = (DivRegReg_t *) bytes;
					
					Register(obj->reg1) /= Register(obj->reg2);
					
					offset += sizeof(DivRegReg_t);
					break;
				}
				/*
				case OP_CanSendRaw11:
				{
					CanSendRaw11_t *obj = (CanSendRaw11_t *) bytes;

					offset += sizeof(CanSendRaw11_t);
					break;
				}
				case OP_CanSendRegVal11:
				{
					CanSendRegVal11_t *obj = (CanSendRegVal11_t *) bytes;

					offset += sizeof(CanSendRegVal11_t);
					break;
				}
				*/
				case OP_Goto:
				{
					Goto_t *obj = (Goto_t *) bytes;

					offset = obj->offset;
					//offset += sizeof(Goto_t);
					break;
				}
				case OP_Exit:
				case 0x00:
				case 0xFF:
				{
					Exit_t *obj = (Exit_t *) bytes;
																					// Как выйти?
					offset += sizeof(Exit_t);
					break;
				}
				default:
				{
					
					// Поиск пользовательских опкодов
					for(auto &obj : _custom_opcode)
					{
						if(obj.opcode == opcode)
						{
							// В функцию нужно прокинуть регистры, чтоюы можно было с ними работать. Как? Сделать вс ерегистры ввиде класса и пробросить объект?
							obj.func(opcode, bytes, offset);

							break;
						}
					}

					// Если найден опкод, который не реализован
					// Нужно что-то делать, иначе зависнем тут навсегда.
					
					break;
				}
			}

			return;
		}

		void SetScriptArg(uint16_t script_id, uint8_t data[4])
		{

		}

		
	private:


		struct custom_opcode_t
		{
			opcode_idx_t opcode;
			custom_opcode_func_t func;
		} _custom_opcode[16];
	
	


};
