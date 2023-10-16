#pragma once

class ScriptReverseLight: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &db_element, StateDB &db_obj, tx_t func) override
		{
			if(db_element.data[0] != 0x61) return;
			
			_tx_packet.id = 0x00E6;
			_tx_packet.raw_data_len = 2;
			_tx_packet.func_id = 0x01;
			if( db_element.data[1] == 0x02 || db_element.data[3] == 0x02 )
			{
				if(_is_active == false)
				{
					_tx_packet.data[0] = 0xFF;
					func(_tx_packet);
					
					_is_active = true;
				}
			}
			else
			{
				if(_is_active == true)
				{
					_tx_packet.data[0] = 0x00;
					func(_tx_packet);
					
					_is_active = false;
				}
			}
			
			return;
		}

	private:
		bool _is_active = false;
};


class ScriptHoodTrunk: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &db_element, StateDB &db_obj, tx_t func) override
		{
			if(db_element.data[0] != 0x65) return;
			if(db_element.data[1] != 0xFF) return;
			
			_tx_packet.raw_data_len = 1;
			_tx_packet.func_id = 0x02;

			switch(id)
			{
				// Кнопка 09, Кнопка открытия капота.
				case 0x012C:
				{
					_tx_packet.id = 0x0185;
					func(_tx_packet);
					
					break;
				}

				// Кнопка 10, Кнопка открытия багажника.
				case 0x012D:
				{
					_tx_packet.id = 0x0184;
					func(_tx_packet);

					break;
				}
			}
			
			return;
		}
};


class ScriptLeftRightDoor: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &db_element, StateDB &db_obj, tx_t func) override
		{
			if(db_element.data[0] != 0x65) return;
			if(db_element.data[1] != 0xFF) return;
			
			_tx_packet.raw_data_len = 1;
			_tx_packet.func_id = 0x03;

			switch(id)
			{
				// Кнопка 11, Кнопка открытия левой двери.
				case 0x012E:
				{
					_tx_packet.id = 0x0187;
					func(_tx_packet);
					
					break;
				}

				// Кнопка 15, Кнопка открытия правой двери.
				case 0x0132:
				{
					_tx_packet.id = 0x0188;
					func(_tx_packet);

					break;
				}
			}
			
			return;
		}
};


class ScriptHorn: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &db_element, StateDB &db_obj, tx_t func) override
		{
			if(db_element.data[0] != 0x65) return;
			
			_tx_packet.id = 0x018B;
			_tx_packet.raw_data_len = 2;
			_tx_packet.func_id = 0x01;
			_tx_packet.data[0] = db_element.data[1];
			func(_tx_packet);
			
			return;
		}
};


class ScriptPowerOnOff: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &db_element, StateDB &db_obj, tx_t func) override
		{
			if(db_element.data[0] != 0x65) return;
			
			bool motor = ((db_element.data[7] >> 4) == 0 || (db_element.data[7] & 0x0F) == 0);
			
			_tx_packet.raw_data_len = 2;
			_tx_packet.func_id = 0x01;
			if(motor == true)
			{
				if(_is_enable == false)
				{
					_tx_packet.data[0] = 0xFF;
					
					_tx_packet.id = 0x0186;
					func(_tx_packet);
					
					_tx_packet.id = 0x018A;
					func(_tx_packet);

					_is_enable = true;
				}
			}
			else
			{
				if(_is_enable == true)
				{
					_tx_packet.data[0] = 0x00;
					
					_tx_packet.id = 0x0186;
					func(_tx_packet);
					
					_tx_packet.id = 0x018A;
					func(_tx_packet);
					
					_is_enable = false;
				}
			}
			
			return;
		}
		
	private:
		bool _is_enable = false;
};
