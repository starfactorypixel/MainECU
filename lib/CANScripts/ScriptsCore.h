#pragma once

class ScriptReverseLight: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &obj, tx_t func) override
		{
			if(obj.data[0] != 0x61) return;
			
			_tx_packet.id = 0x00E6;
			_tx_packet.raw_data_len = 2;
			_tx_packet.func_id = 0x01;
			if( obj.data[1] == 0x02 || obj.data[3] == 0x02 )
			{
				if(active == false)
				{
					_tx_packet.data[0] = 0xFF;
					func(_tx_packet);
					
					active = true;
				}
			}
			else
			{
				if(active == true)
				{
					_tx_packet.data[0] = 0x00;
					func(_tx_packet);
					
					active = false;
				}
			}
			
			return;
		}

	private:
		bool active = false;
};


class ScriptHoodTrunk: public ScriptInterface
{
	public:
		void Run(uint16_t id, StateDB::db_t &obj, tx_t func) override
		{
			if(obj.data[0] != 0x65) return;
			if(obj.data[1] != 0xFF) return;
			
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
		void Run(uint16_t id, StateDB::db_t &obj, tx_t func) override
		{
			if(obj.data[0] != 0x65) return;
			if(obj.data[1] != 0xFF) return;
			
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
