#pragma once
#include <inttypes.h>
#include <L3Constants.h>
#include <L3PacketTypes.h>
#include <CUtils.h>

class L3BlockInfoDB
{
	typedef uint16_t can_id_t;
	
	static constexpr can_id_t _max_can_id = 2048;
	static constexpr uint8_t _max_block = _max_can_id / 16;
	
	using func_cansend_t = void (*)(/*const*/ can_id_t id, /*const*/ uint8_t *data, /*const*/ uint8_t length);
	using callback_update_t = void (*)(/*const */uint8_t *data, uint8_t length);
	using callback_error_t = void (*)(/*const */uint8_t *data, uint8_t length);
	using callback_lost_t = void (*)(/*const */uint8_t *data, uint8_t length);

	// Состояние flags_rx_ переменных, когда полностью получены соотвествующие пакеты.
	static constexpr uint8_t flags_rx_static_filled = (1 << ((sizeof(L3PacketTypes::block_info_t::istatic) + 5) / 6)) - 1;
	static constexpr uint8_t flags_rx_dynamic_filled = (1 << ((sizeof(L3PacketTypes::block_info_t::idynamic) + 5) / 6)) - 1;
	
	struct blocks_info_t
	{
		bool alive;							// Флаг, что блок активен и в сети

		uint8_t flags_rx_static;			// Биты наполнения пакета block_info_static_t
		uint8_t flags_rx_dynamic;			// Биты наполнения пакета block_info_dynamic_t

		uint8_t flag_static_req : 1;		// Флаг, что пакет block_info_static_t был запрошен из L2
		uint8_t flag_static_ready : 1;		// Флаг, что пакет block_info_static_t собран

		uint8_t flag_dynamic_req : 1;		// Флаг, что пакет block_info_dynamic_t был запрошен из L2
		uint8_t flag_dynamic_ready : 1;		// Флаг, что пакет block_info_dynamic_t собран

		uint8_t flag_all_send : 1;			// Флаг, что пакет block_info_static_t + block_info_dynamic_t отправлен в L3
		
		uint32_t static_req_time;
		uint32_t dynamic_req_time;
		
		uint32_t blockinfo_time;			// Время последнего обновления block_info_x
		L3PacketTypes::block_info_t blockinfo;

		uint32_t heartbeat_time;			// Время последнего heartbeat пакета
		uint8_t heartbeat_counter;			// Счётчик последнего heartbeat пакета
	} _blocks[_max_block];

	RingBuffer<_max_block * 4, L3PacketTypes::block_error_t> errors;


	

	
	public:


		
		L3BlockInfoDB() : _blocks{}
		{
			sizeof(_blocks);
			sizeof(L3PacketTypes::block_info_t);
			sizeof(L3PacketTypes::block_error_t);
		}

		void SetCanSendFunc(func_cansend_t callback)
		{
			_FuncCanSend = callback;
			
			return;
		}
		
		void SetUpdateCallback(callback_update_t callback)
		{
			_CallbackUpdate = callback;
			
			return;
		}
		
		void SetErrorCallback(callback_error_t callback)
		{
			_CallbackError = callback;
			
			return;
		}
		
		void SetLostCallback(callback_lost_t callback)
		{
			_CallbackLost = callback;

			return;
		}
		
		void ISR_CanRx(const can_id_t id, const uint8_t *data, const uint8_t length, const uint32_t time)
		{
			if(id >= _max_can_id) return;
			if(data == nullptr) return;
			if(length == 0 || length > 8) return;

			// Если плата состояит из 2 и более секций CAN ID (по 16 каждая которая), если о ошибке сообщений ID не в начальном, первом блоке, то base_id будет вычислен не верно.
			const can_id_t base_id = (id & 0x07F0);		// Если поменяется тип can_id_t, то маска станет не верная. Вынести как константу
			const uint8_t fId = data[0];
			
			switch(fId)
			{
				// block_wakeup_t
				case 0x70:
				{
					break;
				}

				// heartbeat_t
				case 0x71:
				{
					const uint8_t counter = data[1];
					
					blocks_info_t &obj = _blocks[(base_id >> 4)];
					obj.alive = true;
					obj.blockinfo.baseID = base_id;
					obj.heartbeat_time = time;
					obj.heartbeat_counter = counter;
					
					break;
				}

				// block_info_static_t
				case 0x72:
				{
					if(length < 3) return;
					
					const uint8_t page_idx = data[1];
					const uint8_t *data_ptr = &data[2];
					const uint8_t data_len = length - 2;
					const uint16_t rel_idx = page_idx * 6;
					
					blocks_info_t &block = _blocks[(base_id >> 4)];
					
					if(rel_idx + data_len > sizeof(block.blockinfo.istatic)) return;

					uint8_t *write_ptr = (uint8_t *)&block.blockinfo.istatic + rel_idx;
					memcpy(write_ptr, data_ptr, data_len);
					block.blockinfo_time = time;
					block.blockinfo.baseID = base_id;
					block.flags_rx_static |= (1 << page_idx);
					
					// Если мы получили ВСЕ части пакета block_info_static_t
					if(block.flags_rx_static == flags_rx_static_filled)
					{
						block.flag_static_req = 0;
						block.flag_static_ready = 1;
					}
					break;
				}

				// block_info_dynamic_t
				case 0x73:
				{
					if(length < 3) return;
					
					const uint8_t page_idx = data[1];
					const uint8_t *data_ptr = &data[2];
					const uint8_t data_len = length - 2;
					const uint16_t rel_idx = page_idx * 6;
					
					blocks_info_t &block = _blocks[(base_id >> 4)];
					
					if(rel_idx + data_len > sizeof(block.blockinfo.idynamic)) return;
					
					uint8_t *write_ptr = (uint8_t *)&block.blockinfo.idynamic + rel_idx;
					memcpy(write_ptr, data_ptr, data_len);
					block.blockinfo_time = time;
					block.blockinfo.baseID = base_id;
					block.flags_rx_dynamic |= (1 << page_idx);
					
					// Если мы получили ВСЕ части пакета block_info_dynamic_t
					if(block.flags_rx_dynamic == flags_rx_dynamic_filled)
					{
						block.flag_dynamic_req = 0;
						block.flag_dynamic_ready = 1;
					}
					
					break;
				}

				// block_error_t
				case 0x75:
				{
					if(length != 5) return;
					
					const uint8_t *data_ptr = &data[1];
					const uint8_t data_len = length - 1;
					
					if(sizeof(L3PacketTypes::block_error_t::error) != data_len) return;
					
					L3PacketTypes::block_error_t error = {};
					error.baseId = base_id;
					error.realId = id;
					memcpy((uint8_t *)&error.error, data_ptr, data_len);

					errors.Write(error);
					
					break;
				}

				// Не реализована логика сбора ошибок от конкретных ID а не от блока.

				default:
					return;
			}
		}

/*
		template<typename Func> 
		void GetAllIter(Func &&callback)
		{
			for(auto &block : _blocks)
			{
				//if(block.flags == 0x00) continue;

				callback((uint8_t *)&block.blockinfo, sizeof(block.blockinfo));
			}
			
			return;
		}
*/
		
		void Processing(uint32_t time)
		{
			for(auto &block : _blocks)
			{
				if(block.alive == true && time - block.heartbeat_time >= 10500)
				{
					block.alive = false;

					block.flags_rx_static = 0;
					block.flags_rx_dynamic = 0;
					block.flag_static_req = 0;
					block.flag_static_ready = 0;
					block.flag_dynamic_req = 0;
					block.flag_dynamic_ready = 0;
					block.flag_all_send = 0;
					
					if(_CallbackLost != nullptr)
						_CallbackLost((uint8_t *)&block.blockinfo, sizeof(block.blockinfo));
				}
				
				if(block.alive == true)
				{
					// Если был отправлен запрос на получение static части
					if(block.flag_static_req == 1)
					{
						// Если вышло время ожидания static части
						if(time - block.static_req_time >= 1000)
						{
							block.flags_rx_static = 0;
							block.flag_static_req = 0;
							block.flag_static_ready = 0;
						}
					}
					
					// Если был отправлен запрос на получение dynamic части
					if(block.flag_dynamic_req == 1)
					{
						// Если вышло время ожидания dynamic части
						if(time - block.dynamic_req_time >= 1000)
						{
							block.flags_rx_dynamic = 0;
							block.flag_dynamic_req = 0;
							block.flag_dynamic_ready = 0;
							block.flag_all_send = 0;
						}
					}
					
					// Если запроса static части небыло и static часть не получена полностью
					if(block.flag_static_req == 0 && block.flags_rx_static != flags_rx_static_filled)
					{
						block.flags_rx_static = 0;
						block.flag_static_req = 1;
						block.flag_static_ready = 0;
						block.static_req_time = time;
						
						uint8_t packet[] = {0x32};
						_FuncCanSend(block.blockinfo.baseID, packet, sizeof(packet));
					}
					
					// Если запроса dynamic части не было
					if(block.flag_dynamic_req == 0)
					{
						// Если пора запрашивать dynamic часть
						if(time - block.blockinfo_time >= 5000)
						{
							block.flags_rx_dynamic = 0;
						}
						
						// Если dynamic часть не полная
						if(block.flags_rx_dynamic != flags_rx_dynamic_filled)
						{
							block.flags_rx_dynamic = 0;
							block.flag_dynamic_req = 1;
							block.flag_dynamic_ready = 0;
							block.flag_all_send = 0;
							block.dynamic_req_time = time;
							
							uint8_t packet[] = {0x33};
							_FuncCanSend(block.blockinfo.baseID, packet, sizeof(packet));
						}
					}
					
					// Если получены все части static и dynamic
					if(block.flag_static_ready == 1 && block.flag_dynamic_ready == 1)
					{
						if(block.flag_all_send == 0)
						{
							block.flag_all_send = 1;
							
							if(_CallbackUpdate != nullptr)
								_CallbackUpdate((uint8_t *)&block.blockinfo, sizeof(block.blockinfo));
						}
					}
				}
			}

			uint8_t error_count = errors.Count();
			while(error_count-- > 0)
			{
				L3PacketTypes::block_error_t obj;
				if(errors.Read(obj) == false) break;
				
				_CallbackError((uint8_t *)&obj, sizeof(obj));
			}
			
			return;
		}
		
	private:

		func_cansend_t _FuncCanSend = nullptr;
		callback_update_t _CallbackUpdate = nullptr;
		callback_error_t _CallbackError = nullptr;
		callback_lost_t _CallbackLost = nullptr;
};
