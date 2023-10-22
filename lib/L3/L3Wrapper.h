/*
	Обёртка пакета L3.
*/

#pragma once

#include <L3Constants.h>
#include <L3Packet.h>
#include <L3PacketTypes.h>
#include <L3Driver.h>

class L3Wrapper
{
	static const uint8_t _max_dev = 4;
	
	public:
		using packet_t = L3Packet<L3PacketDataSize>;
		using callback_event_t = bool (*)(L3DevType_t dev, packet_t &request, packet_t &response);
		using callback_error_t = void (*)(L3DevType_t dev, packet_t &request, int8_t code);
		using callback_reset_t = void (*)(L3DevType_t dev);
		
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
		
		void RegCallback(callback_event_t event, callback_error_t error, callback_reset_t reset)
		{
			this->_callback_event = event;
			this->_callback_error = error;
			this->_callback_reset = reset;

			return;
		}
		
		void Processing(uint32_t time)
		{
			for(uint8_t i = 0; i < _dev_obj_idx; ++i)
			{
				_object_t &obj = _dev_obj[i];
				
				if( obj.driver->NeedGetPacket() == true )
				{
					obj.driver->GetPacket( obj.rx_packet );
					
					if( obj.rx_packet.GetError() == obj.rx_packet.ERROR_NONE )
					{
						obj.state = L3_DEVSTATE_ACTIVE;
						obj.ping_attempts = 0;


						DEBUG_LOG_TOPIC("L3_ROOT", "RawPacket(%d): ", obj.rx_packet.GetPacketLength());
						DEBUG_LOG_ARRAY_HEX(nullptr, obj.rx_packet.GetPacketPtr(), obj.rx_packet.GetPacketLength());
						DEBUG_LOG_SIMPLE(";\n");


						// Тут выполняем все 'системные' вызовы, а всё остальное отправляем в callback.
						switch ( obj.rx_packet.Type() )
						{
							// !!!!!!!!!! Сейчас рукопожатие не обязательное. Наверное стоит принудительно не обрабатывать запросы если его не было.
							case L3_REQTYPE_SERVICES:
							{
								if( obj.rx_packet.Param() == 0x0000 )
								{
									_SendHandshake(obj);
								}
								else if( obj.rx_packet.Param() == 0x0001 )
								{

									uint8_t *packet_ptr = obj.rx_packet.GetDataPtr();
									if(packet_ptr[0] == 0x02 && obj.rx_packet.GetDataLength() == sizeof(L3PacketTypes::auth_init_req_t))
									{
										L3PacketTypes::auth_init_req_t *packet_auth = (L3PacketTypes::auth_init_req_t *)packet_ptr;
										
										_SendAuthInit(obj);
									}
									else if(packet_ptr[0] == 0x04 && obj.rx_packet.GetDataLength() == sizeof(L3PacketTypes::auth_req_t))
									{
										L3PacketTypes::auth_req_t *packet_auth = (L3PacketTypes::auth_req_t *)packet_ptr;
										if( Security::CheckAuth(packet_auth) == true )
										{
											obj.auth = true;
											_SendAuthResult(obj, 1);
										}
										else
										{
											obj.auth = false;
											_SendAuthResult(obj, 0);
										}
									}

								}
								else if( obj.rx_packet.Param() == 0xFFFF )
								{
									// Ответ на пинг
									DEBUG_LOG_TOPIC("L3_ROOT", "Ping RX;\n");
								}

								break;
							}
							default:
							{
								if(obj.auth == false) break;
								
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
						if( (time - obj.rx_packet.GetPacketTime()) > L3DevicePingInterval )
						{
							DEBUG_LOG_TOPIC("L3_ROOT", "Ping TX;\n");
							obj.state = L3_DEVSTATE_PING;
							obj.ping_attempts++;
							
							_SendPing(obj);
						}
						
						break;
					}
					case L3_DEVSTATE_PING:
					{
						if( (time - obj.rx_packet.GetPacketTime()) > (L3DevicePingInterval * (obj.ping_attempts + 1)) )
						{
							if(obj.ping_attempts == L3DevicePingCount)
							{
								DEBUG_LOG_TOPIC("L3_ROOT", "Ping Timeout;\n");
								obj.state = L3_DEVSTATE_TIMEOUT;
							}
							else
							{
								DEBUG_LOG_TOPIC("L3_ROOT", "Ping TX %d;\n", obj.ping_attempts);
								obj.ping_attempts++;
								
								_SendPing(obj);
							}
						}

						break;
					}
					case L3_DEVSTATE_TIMEOUT:
					{
						DEBUG_LOG_TOPIC("L3_ROOT", "Ping end, _ResetDevice();\n");
						_ResetDevice(obj);

						break;
					}
				}
				
			}
			
			return;
		}
		
		void Send(L3DevType_t dev_type, uint8_t type, uint16_t param, byte *data, uint8_t length)
		{
			for(uint8_t i = 0; i < _max_dev; ++i)
			{
				_object_t &obj = _dev_obj[i];
				
				if(obj.auth == false) continue;
				//if( dev_type == L3_DEVTYPE_ALL || obj.driver->GetType() == dev_type )
				if( (obj.driver->GetType() & dev_type) > 0 )
				{
					obj.tx_packet.Type(type);
					obj.tx_packet.Param(param);
					for(int8_t i = 0; i < length; ++i)
					{
						obj.tx_packet.PutData(data[i]);
					}
					this->_Send(obj);
					
					//break;
				}
			}
			
			return;
		}
		
	private:

		// Вот как перенести это ниже _Send() избавится от ошибки невидимости..
		struct _object_t
		{
			bool auth;				// Флаг авторизированного устройства.
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
			obj.tx_packet.Type(L3_REQTYPE_SERVICES);
			obj.tx_packet.Param(0x0000);
			
			return _Send(obj);
		}
		
		
		
		
		
		
		
		void _SendAuthInit(_object_t &obj)
		{
			L3PacketTypes::auth_init_resp_t packet;
			packet.funcID = 0x03;
			packet.method = 0x01;
			
			uint8_t hwmac[8];
			esp_efuse_mac_get_default(hwmac);
			memcpy(packet.devID, hwmac, sizeof(packet.devID));
			
			obj.tx_packet.Type(L3_REQTYPE_SERVICES);
			obj.tx_packet.Param(0x0001);
			obj.tx_packet.PutData( (uint8_t *)&packet, sizeof(packet) );
			
			return _Send(obj);
		}
		
		void _SendAuthResult(_object_t &obj, uint8_t code)
		{
			L3PacketTypes::auth_resp_t packet;
			packet.funcID = 0x05;
			packet.code = code;
			packet.time = millis();
			
			obj.tx_packet.Type(L3_REQTYPE_SERVICES);
			obj.tx_packet.Param(0x0001);
			obj.tx_packet.PutData( (uint8_t *)&packet, sizeof(packet) );
			
			return _Send(obj);
		}
		
		
		void _SendPing(_object_t &obj)
		{
			obj.tx_packet.Type(L3_REQTYPE_SERVICES);
			obj.tx_packet.Param(0xFFFF);
			
			uint32_t tmp1 = millis();
			uint8_t *tmp2 = (uint8_t *) &tmp1;
			obj.tx_packet.PutData(tmp2, 4);
			
			return _Send(obj);
		}
		
		void _ResetDevice(_object_t &obj)
		{
			obj.auth = false;
			obj.state = L3_DEVSTATE_IDLE;
			obj.driver->Reset();
			obj.rx_packet.Init();
			obj.tx_packet.Init();
			obj.ping_attempts = 0;

			this->_callback_reset( obj.driver->GetType() );
			
			return;
		}
		
		callback_event_t _callback_event;
		callback_error_t _callback_error;
		callback_reset_t _callback_reset;
		_object_t _dev_obj[_max_dev];
		uint8_t _dev_obj_idx = 0;
		
};