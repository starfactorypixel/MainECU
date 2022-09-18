/*
	Обёртка пакета L3.
*/

#pragma once

#include <L3Constants.h>
#include <L3Packet.h>
#include <L3Driver.h>

class L3Wrapper
{
	static const uint8_t _max_dev = 4;
	
	public:
		using packet_t = L3Packet<L3PacketDataSize>;
		using callback_event_t = bool (*)(L3DevType_t dev, packet_t &request, packet_t &response);
		using callback_error_t = void (*)(L3DevType_t dev, packet_t &request, int8_t code);
		
		L3Wrapper()
		{
			return;
		}
		
		bool AddDevice(L3Driver &driver)
		{
			bool result = false;
			
			if(_dev_obj_idx < _max_dev)
			{
				_dev_obj[_dev_obj_idx].state = L3_DEVSTATE_IDLE;
				_dev_obj[_dev_obj_idx].driver = &driver;
				++_dev_obj_idx;
				
				result = true;
			}
			
			return result;
		}
		
		void Init()
		{
			for(uint8_t i = 0; i < _dev_obj_idx; ++i)
			{
				_dev_obj[i].driver->Init();
			}
			
			return;
		}
		
		void RegCallback(callback_event_t event, callback_error_t error = nullptr)	// Добавить тип устройства
		{
			this->_callback_event = event;
			this->_callback_error = error;

			return;
		}
		
		// В будущем будет заменён на прерывание приёма байта.
		void Processing(uint32_t time)
		{
			for(uint8_t i = 0; i < _dev_obj_idx; ++i)
			{
				_object_t &obj = _dev_obj[i];
				
				// Пока не перейдём на работу с регистрами, эмитирует их таким образом //
				obj.driver->Tick(time);
				// //
				
				if( obj.driver->NeedGetPacket() == true )
				{
					obj.driver->GetPacket( obj.rx_packet );
					
					if( obj.rx_packet.GetError() == obj.rx_packet.ERROR_NONE )
					{
						obj.state = L3_DEVSTATE_ACTIVE;
						obj.ping_attempts = 0;
						
						// Тут выполняем все 'системные' вызовы, а всё остальное отправляем в callback.
						switch ( obj.rx_packet.Type() )
						{
							// !!!!!!!!!! Сейчас рукопожатие не обязательное. Наверное стоит принудительно не обрабатывать запросы если его не было.
							case L3_REQTYPE_HANDSHAKE:
							{
								if( obj.rx_packet.Param() == 0x0000 )
								{
									_SendHandshake(obj); break;
								}
								else if( obj.rx_packet.Param() == 0xFFFF )
								{
									// Ответ на пинг
								}

								break;
							}
							default:
							{
								if( _callback_event( obj.driver->GetType(), obj.rx_packet, obj.tx_packet ) == true )
								{
									this->_Send(obj);
								}
								
								break;
							}
						}
					}
					else
					{
						if(this->_callback_error != nullptr)
						{
							this->_callback_error(obj.driver->GetType(), obj.rx_packet, obj.rx_packet.GetError());
						}
						
						// Или метод FlushBuffer() ?
						while(obj.driver->ReadAvailable() > 0){ obj.driver->ReadByte(); }
					}
					
					//obj.rx_packet.Init();
				}
				
				
				switch (obj.state)
				{
					case L3_DEVSTATE_ACTIVE:
					{
						if( (time - obj.rx_packet.GetPacketTime()) > 1500 )
						{
							Serial.println("obj.state == L3_DEVSTATE_ACTIVE. Send Ping");
							obj.state = L3_DEVSTATE_PING;
							obj.ping_attempts++;
							
							_SendPing(obj);
						}
						
						break;
					}
					case L3_DEVSTATE_PING:
					{
						if( (time - obj.rx_packet.GetPacketTime()) > (1500 * obj.ping_attempts) )
						{
							if(obj.ping_attempts == 3)
							{
								Serial.println("ping_attempts == 3");
								obj.state = L3_DEVSTATE_TIMEOUT;
							}
							else
							{
								Serial.println("ping_attempts < 3");
								obj.ping_attempts++;
								
								_SendPing(obj);
							}
						}

						break;
					}
					case L3_DEVSTATE_TIMEOUT:
					{
						Serial.println("_ResetDevice()");
						_ResetDevice(obj);

						break;
					}
				}
			}
			
			return;
		}
		
		void Send(uint8_t id_dev, uint8_t type, uint16_t param, byte *data, uint8_t length)	// *&data
		{
			if(id_dev < _max_dev)
			{
				_object_t &obj = _dev_obj[id_dev];
				
				obj.tx_packet.Type(type);
				obj.tx_packet.Param(param);
				for(int8_t i = 0; i < length; ++i)
				{
					obj.tx_packet.PutData(data[i]);
				}
				this->_Send(obj);
			}
			
			return;
		}
		
	private:

		// Вот как перенести это ниже _Send() избавится от ошибки невидимости..
		struct _object_t
		{
			L3DevState_t state;		// Состояние устройства.
			L3Driver *driver;		// Объект низкоуровневого драйвера устройства.
			packet_t rx_packet;		// Объект принимаемого пакета.
			packet_t tx_packet;		// Объект отправляемого пакета.
			uint8_t ping_attempts;	// Кол-во попыток получить пинг.
		};
		
		void _Send(_object_t &obj)
		{
			obj.tx_packet.Direction(0x01);
			
			obj.driver->PutPacket( obj.tx_packet );
			
			obj.tx_packet.Init();
			
			return;
		}
		
		void _SendHandshake(_object_t &obj)
		{
			obj.tx_packet.Type(L3_REQTYPE_HANDSHAKE);
			obj.tx_packet.Param(0x0000);
			
			return _Send(obj);
		}
		
		void _SendPing(_object_t &obj)
		{
			obj.tx_packet.Type(L3_REQTYPE_HANDSHAKE);
			obj.tx_packet.Param(0xFFFF);
			
			return _Send(obj);
		}
		
		void _ResetDevice(_object_t &obj)
		{
			obj.state = L3_DEVSTATE_IDLE;
			obj.driver->Reset();
			obj.rx_packet.Init();
			obj.tx_packet.Init();
			obj.ping_attempts = 0;
			
			return;
		}
		
		callback_event_t _callback_event;
		callback_error_t _callback_error;
		_object_t _dev_obj[_max_dev];
		uint8_t _dev_obj_idx = 0;
		
};