#pragma once
#include <inttypes.h>
#include <L3Constants.h>
#include <L3PacketTypes.h>

class L3BlockInfoDB
{
	typedef uint16_t can_id_t;
	
	static constexpr can_id_t _max_can_id = 2048;
	static constexpr uint8_t _max_block = _max_can_id / 16;
	
	using callback_lost_t = void (*)(/*const */uint8_t *data, uint8_t length);
	using callback_update_t = void (*)(/*const */uint8_t *data, uint8_t length);
	
	struct blocks_info_t
	{
		uint8_t flags: 7;			// Флаги заполнения структуры
		uint8_t update : 1;			// Флаг обновлённых но не отправленных данных
		uint32_t time;				// Время обновления данных
		union
		{
			uint8_t raw[sizeof(L3PacketTypes::block_info_t)];
			L3PacketTypes::block_info_t obj;
		};
		
	} _blocks[_max_block];

	
	public:


		
		L3BlockInfoDB() : _blocks{}
		{}

		void SetLostCallback(callback_lost_t callback)
		{
			_callback_lost = callback;

			//sizeof(_blocks);
			
			return;
		}
		
		void SetUpdateCallback(callback_update_t callback)
		{
			_callback_update = callback;
			
			return;
		}
		
		void PutAutoTemp(can_id_t id, uint8_t *data, uint8_t length, uint32_t time)
		{

			PutAuto(id, data, length, time);
		}
		
		void PutAuto(can_id_t id, uint8_t *data, uint8_t length, uint32_t time)
		{
			if(id >= _max_can_id) return;
			if(data == nullptr) return;
			
			can_id_t base_id = (id & 0x07F0);
			uint8_t offset_id = (id & 0x000F);
			blocks_info_t &obj = _blocks[(base_id >> 4)];
			
			bool is_ok = false;
			switch(offset_id)
			{
				case 0: is_ok = _PutBlockInfo(obj, data, length); break;
				case 1: is_ok = _PutBlockHealth(obj, data, length); break;
				case 2: is_ok = _PutBlockFeatures(obj, data, length); break;
				default: break;
			}
			if(is_ok == true)
			{
				obj.flags |= (1 << offset_id);
				obj.update = 1;
				obj.time = time;
				obj.obj.baseID = base_id;
			}
			
			return;
		}
		
		template<typename Func> 
		void GetAllIter(Func &&callback)
		{
			for(auto &block : _blocks)
			{
				if(block.flags == 0x00) continue;

				callback(block.raw, sizeof(block.raw));
			}
			
			return;
		}
		
		void Processing(uint32_t time)
		{
			for(auto &block : _blocks)
			{
				if(time - block.time > 20000UL && block.flags != 0)
				{
					block.flags = 0x00;
					block.update = 0;
					if(_callback_lost != nullptr)
						_callback_lost(block.raw, sizeof(block.raw));
				}
				
				if(block.update && block.flags > 0/* == 0b0000111*/)
				{
					block.update = 0;
					if(_callback_update != nullptr)
						_callback_update(block.raw, sizeof(block.raw));
				}
			}
			
			return;
		}
		
	private:
		
		bool _PutBlockInfo(blocks_info_t &obj, uint8_t *data, uint8_t length)
		{
			if(length != sizeof(l2_block_info_t)) return false;
			
			l2_block_info_t *l2_obj = (l2_block_info_t *)data;

			obj.obj.hw_type = l2_obj->hw_type;
			obj.obj.hw_ver = l2_obj->hw_ver;
			obj.obj.sw_ver = l2_obj->sw_ver;
			obj.obj.can_ver = l2_obj->can_ver;
			obj.obj.uptime = l2_obj->uptime;
			
			return true;
		}
		
		bool _PutBlockHealth(blocks_info_t &obj, uint8_t *data, uint8_t length)
		{
			if(length != sizeof(l2_block_health_t)) return false;
			
			l2_block_health_t *l2_obj = (l2_block_health_t *)data;

			obj.obj.voltage = l2_obj->voltage;
			obj.obj.current = l2_obj->current;
			obj.obj.temperature = l2_obj->temperature;
			obj.obj.error_flags = l2_obj->error_flags;

			return true;
		}
		
		bool _PutBlockFeatures(blocks_info_t &obj, uint8_t *data, uint8_t length)
		{
			if(length != sizeof(l2_block_features_t)) return false;
			
			l2_block_features_t *l2_obj = (l2_block_features_t *)data;
			
			memcpy(obj.obj.features, l2_obj->data, sizeof(l2_obj->data));
			
			return true;
		}
		







		struct __attribute__((packed)) l2_block_info_t
		{
			uint8_t fId;
			uint8_t hw_ver : 3;			// Версия платы, 3 бита
			uint8_t hw_type : 5;		// Тип платы, 5 бит
			uint8_t can_ver : 2;		// Версия протокола CAN, 2 бита
			uint8_t sw_ver : 6;			// Версия программы, 6 бит
			uint32_t uptime;			// Uptime блока, мс.
			uint8_t empty8;
		};

		struct __attribute__((packed)) l2_block_health_t
		{
			uint8_t fId;
			uint16_t voltage;			// Напряжение питание блока
			uint16_t current;			// Общий потребляемый ток блока
			int8_t temperature;			// Температура блока, если есть
			uint8_t error_flags;		// Флаги налчичия ошибок блока
			uint8_t empty8;
		};
		
		struct __attribute__((packed)) l2_block_features_t
		{
			uint8_t fId;
			uint8_t data[7];
		};


		callback_lost_t _callback_lost = nullptr;
		callback_update_t _callback_update = nullptr;


};
