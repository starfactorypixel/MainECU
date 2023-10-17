#pragma once

#include <inttypes.h>
#include <L2Wrapper.h>


typedef std::function<void(L2Wrapper::packet_v2_t &can_obj)> tx_t;


class ScriptInterface
{
	public:
		virtual ~ScriptInterface() = default;
		
		virtual void Run(uint16_t id, StateDB::db_t &db_element, tx_t func) = 0;
		
		static inline StateDB *db_obj;
		
	protected:
		L2Wrapper::packet_v2_t _tx_packet;
		
};

#include "ScriptsCore.h"
#include "ScriptsLight.h"


class CANScripts
{
	public:

		CANScripts(L2Wrapper *l2_obj, StateDB *db_obj) : _l2_obj(l2_obj)
		{
			memset(&_obj, 0x00, sizeof(_obj));
			
			// Костыль инициализации статического объекта в ScriptInterface.
			tmp->db_obj = db_obj;
			
			// Обработка 'запуска' двигателя
			_obj[0x0101] = new ScriptPowerOnOff();
			
			// Передача и фактическое направление вращения колёс
			_obj[0x010A] = new ScriptReverseLight();

			// Кнопка 01, Ближний свет.
			_obj[0x0124] = new ScriptSideLowHighBeam();
			
			// Кнопка 02, Габариты.
			_obj[0x0125] = new ScriptSideLowHighBeam();
			
			// Кнопка 03, Свет в салоне.
			_obj[0x0126] = new ScriptCabinLight();
			
			// Кнопка 04, Клаксон.
			_obj[0x0127] = new ScriptHorn();
			
			// Кнопка 05, Аварийка.
			_obj[0x0128] = new ScriptLeftRightHazard();
			
			// Кнопка 06, Вентилятор.
			_obj[0x0129] = nullptr;
			
			// Кнопка 07, Левый поворотник.
			_obj[0x012A] = new ScriptLeftRightHazard();
			
			// Кнопка 08, Педаль тормоза.
			_obj[0x012B] = new ScriptBrakeLight();
			
			// Кнопка 09, Кнопка открытия капота.
			_obj[0x012C] = new ScriptHoodTrunk();
			
			// Кнопка 10, Кнопка открытия багажника.
			_obj[0x012D] = new ScriptHoodTrunk();
			
			// Кнопка 11, Кнопка открытия левой двери.
			_obj[0x012E] = new ScriptLeftRightDoor();
			
			// Кнопка 12, Правый поворотник.
			_obj[0x012F] = new ScriptLeftRightHazard();
			
			// Кнопка 13, Концевик левая дверь.
			_obj[0x0130] = new ScriptCabinLight();
			
			// Кнопка 14, Концевик правая дверь.
			_obj[0x0131] = new ScriptCabinLight();
			
			// Кнопка 15, Кнопка открытия правой двери.
			_obj[0x0132] = new ScriptLeftRightDoor();
			
			// Кнопка 16, Дальний свет.
			_obj[0x0133] = new ScriptSideLowHighBeam();
			
			// Подрулевой переключатель 1, йййй.
			_obj[0x0134] = nullptr;

			// Подрулевой переключатель 2, йййй.
			_obj[0x0135] = nullptr;
			
			return;
		}

		void Processing(uint16_t id, StateDB::db_t &db_element)
		{
			if(id >= 2048) return;
			if(_obj[id] == 0 /*nullptr*/) return;

			_obj[id]->Run(id, db_element, [&](L2Wrapper::packet_v2_t &can_obj)
			{
				_l2_obj->Send(can_obj);
			});
			
			return;
		}

	private:
		ScriptInterface *_obj[2048];
		L2Wrapper *_l2_obj;
		
		ScriptInterface *tmp;

};
