#pragma once

class ScriptLeftRightHazard: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &db_element, tx_t func) override
		{
			if(db_element.data[0] != 0x65) return;
			
			_tx_packet.raw_data_len = 2;
			_tx_packet.func_id = 0x01;
			_tx_packet.data[0] = db_element.data[1];

			switch(id)
			{
				// Кнопка 07, Левый поворотник.
				case 0x012A:
				{
					_tx_packet.id = 0x00C7;
					func(_tx_packet);
					_tx_packet.id = 0x00E7;
					func(_tx_packet);
					
					break;
				}

				// Кнопка 12, Правый поворотник.
				case 0x012F:
				{
					_tx_packet.id = 0x00C8;
					func(_tx_packet);
					_tx_packet.id = 0x00E8;
					func(_tx_packet);

					break;
				}

				// Кнопка 05, Аварийка.
				case 0x0128:
				{
					_tx_packet.id = 0x00C9;
					func(_tx_packet);
					_tx_packet.id = 0x00E9;
					func(_tx_packet);
					
					break;
				}
			}
			
			return;
		}
};


class ScriptSideLowHighBeam: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &db_element, tx_t func) override
		{
			if(db_element.data[0] != 0x65) return;
			
			_tx_packet.raw_data_len = 2;
			_tx_packet.func_id = 0x01;
			_tx_packet.data[0] = db_element.data[1];

			switch(id)
			{
				// Кнопка 02, Габариты.
				case 0x0125:
				{
					_tx_packet.id = 0x00C4;
					func(_tx_packet);
					_tx_packet.id = 0x00E4;
					func(_tx_packet);
					
					break;
				}

				// Кнопка 01, Ближний свет.
				case 0x0124:
				{
					_tx_packet.id = 0x00C5;
					func(_tx_packet);

					break;
				}

				// Кнопка 16, Дальний свет.
				case 0x0133:
				{
					_tx_packet.id = 0x00C6;
					func(_tx_packet);
					
					break;
				}
			}
			
			return;
		}
};


class ScriptBrakeLight: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &db_element, tx_t func) override
		{
			if(db_element.data[0] != 0x65) return;
			
			_tx_packet.id = 0x00E5;
			_tx_packet.raw_data_len = 2;
			_tx_packet.func_id = 0x01;
			_tx_packet.data[0] = db_element.data[1];
			func(_tx_packet);
			
			return;
		}
};


class ScriptCabinLight: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &db_element, tx_t func) override
		{
			if(db_element.data[0] != 0x65) return;

			_tx_packet.id = 0x0189;
			_tx_packet.raw_data_len = 2;
			_tx_packet.func_id = 0x01;
			_tx_packet.data[0] = db_element.data[1];
			func(_tx_packet);
			
			return;
		}
};
