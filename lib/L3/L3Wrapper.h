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
		using packet_t = L3Packet<64>;
		using callback_event_t = bool (*)(L3DevType_t dev, packet_t &request, packet_t &response);
		using callback_error_t = void (*)(L3DevType_t dev, packet_t &request, int8_t code);
		
		L3Wrapper(uint8_t transport)
		{
			this->_transport = transport;
			
			/*
			switch(transport)
			{
				case 0x00: { _rx_packet.SetTimeout(25); break; };
				case 0x01: { _rx_packet.SetTimeout(5);  break; };
				case 0x02: { _rx_packet.SetTimeout(25); break; };
				case 0x03: { _rx_packet.SetTimeout(10); break; };
			}
			*/
			
			return;
		}

		bool AddDevice(L3DevType_t type)
		{
			bool result = false;
			
			if(_dev_obj_idx < _max_dev)
			{
				_dev_obj[_dev_obj_idx].type = type;
				_dev_obj[_dev_obj_idx].state = L3_STATE_IDLE;
				switch(type)
				{
					case L3_DEVTYPE_BLUETOOTH:
					{
						_dev_obj[_dev_obj_idx].driver = new L3DriverBluetooth();
						_dev_obj[_dev_obj_idx].rx_packet.SetTimeout(25);
						break;
					}
					case L3_DEVTYPE_DASHBOARD:
					{
						_dev_obj[_dev_obj_idx].driver = new L3DriverSerial();
						_dev_obj[_dev_obj_idx].rx_packet.SetTimeout(10);
						break;
					}
					case L3_DEVTYPE_COMPUTER:
					{
						_dev_obj[_dev_obj_idx].driver = new L3DriverSerial();
						_dev_obj[_dev_obj_idx].rx_packet.SetTimeout(10);
						break;
					}
				}
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
		
		void SetUrgent()
		{
			this->_urgent_data = true;
			
			return;
		}
		
		// В будущем будет заменён на прерывание приёма байта.
		void Processing(uint32_t time)
		{
			for(uint8_t i = 0; i < _dev_obj_idx; ++i)
			{
				_object_t &obj = _dev_obj[i];
				
				if( obj.driver->ReadAvailable() > 0 )
				{
					byte incoming_byte = obj.driver->ReadByte();
					
					if( obj.rx_packet.PutPacketByte(incoming_byte, millis()) == true )
					{
						if( obj.rx_packet.IsReceived() == true )
						{
							// Тут выполняем все 'системные' вызовы, а всё остальное отправляем в callback.
							switch ( obj.rx_packet.Type() )
							{
								case L3_REQTYPE_HANDSHAKE:
								{
									obj.tx_packet.Type( obj.rx_packet.Type() );
									this->_Send(obj);
									
									break;
								}
								/*
								case L3_REQTYPE_REGID:
								{
									
									
									break;
								}
								*/
								default:
								{
									if( _callback_event(obj.type, obj.rx_packet, obj.tx_packet) == true )
									{
										this->_Send(obj);
									}
									
									break;
								}
							}
							
							obj.rx_packet.Init();
						}
					}
					
					if(obj.rx_packet.GetError() < 0)
					{
						if(this->_callback_error != nullptr)
						{
							this->_callback_error(obj.type, obj.rx_packet, obj.rx_packet.GetError());
						}
						obj.rx_packet.Init();
						
						// Или метод FlushBuffer() ?
						while(obj.driver->ReadAvailable() > 0){ obj.driver->ReadByte(); }
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
			L3DevType_t type;		// Тип устройства.
			L3State_t state;		// Состояние устройства.
			L3Driver *driver;		// Объект низкоуровневого драйвера устройства.
			packet_t rx_packet;		// Объект принимаемого пакета.
			packet_t tx_packet;		// Объект отправляемого пакета.
		};
		
		void _Send(_object_t &obj)
		{
			
			obj.tx_packet.Transport(this->_transport);
			obj.tx_packet.Direction(0x01);
			
			if(this->_urgent_data == true) obj.tx_packet.Urgent(1);
			this->_urgent_data = false;
			
			obj.tx_packet.Prepare();
			
			obj.driver->SendBytes( obj.tx_packet.GetPacketPtr(), obj.tx_packet.GetPacketLength() );
			
			obj.tx_packet.Init();
			
			return;
		}
		
		callback_event_t _callback_event;
		callback_error_t _callback_error;
		
		//L3Driver *_driver;
		
		uint8_t _transport;
		bool _urgent_data = false;



		_object_t _dev_obj[_max_dev];
		uint8_t _dev_obj_idx = 0;


		



};
