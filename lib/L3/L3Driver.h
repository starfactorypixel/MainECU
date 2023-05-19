/*
	Драйвера физического уровня для протокола L3.
	Работают с регистрами, прерываниями и с горячими пакета данных.
*/

#pragma once

class L3Driver
{
	using packet_t = L3Packet<L3PacketDataSize>;
	
	public:
		
		virtual void Init() = 0;
		virtual uint8_t ReadAvailable() = 0;
		virtual uint8_t ReadByte() = 0;
		virtual void SendByte(uint8_t data) = 0;
		virtual void SendBytes(const uint8_t *buffer, uint8_t length) = 0;
		void Reset(){};
		
		// Временный метод, эмитирующий прерывания.
		void Tick(uint32_t time)
		{
			
			// 'Типа' прерывание по приёму байта.
			// Если хоть что то есть, то принимаем до дого момента как пакет будет полностью получен,
			// после чего выходим и даём время на остальные обработки.
			uint8_t rx_data = 0x00;
			while(ReadAvailable() > 0)
			{
				// Если пакет уже принят и ещё не обработан, то игнорируем. Увы, может быть потеря, иначе никак.
				if(_rx_packet.IsReceived() == true) break;
				
				rx_data = ReadByte();
				if( _rx_packet.PutPacketByte(rx_data, time) == true )
				{
					// Nothing?
				}
			}
			
			// 'Типа' прерывание по передачи байта.
			if(_tx_packet.IsPrepared() == true)
			{
				uint8_t *tx_data_ptr = _tx_packet.GetPacketPtr();
				SendBytes(tx_data_ptr, _tx_packet.GetPacketLength());

/*				while(_tx_packet.GetPacketByte(inout_byte) == true)
				{
					SendByte(inout_byte);
				}
*/
				_tx_packet.Init();
			}
			
			return;
		}
		
		// Флаг того, что пакет можно забирать. Это или принятый пакет или пакет с ошибкой.
		bool NeedGetPacket()
		{
			return (_rx_packet.IsReceived() == true || _rx_packet.GetError() != _rx_packet.ERROR_NONE);
		}
		
		// Флаг того, что можно вставлять пакет для отправки.
		bool CanPutPacket()
		{
			return _tx_packet.IsReady();
		}
		
		// Копирует принятый пакет в тот что передан по ссылке и очищает горячий пакет.
		void GetPacket(packet_t &packet)
		{
			packet = _rx_packet;
			_rx_packet.Init();
			
			return;
		}
		
		// Копирует отправляемый пакет из того что передан по ссылке и подготавливает его к отправке.
		void PutPacket(packet_t &packet)
		{
			//Serial.println("PutPacket 1");
			// Временный костыль: Вешаем поток до тех пор, пока предыдущий пакет не будет отправлен полностью.
			while(_tx_packet.IsReady() == false)
			{
				delayMicroseconds(50);
				
				/*
				Serial.println( (_tx_packet.IsPrepared() == true), HEX );
				Serial.println(_tx_packet.Type(), HEX);
				Serial.println(_tx_packet.Param(), HEX);
				Serial.println(_tx_packet.GetDataLength(), HEX);
				*/
			}
			//if(_tx_packet.IsReady() == false) return;

			//Serial.println("PutPacket 2");

			_tx_packet = packet;
			_tx_packet.Prepare();

			//Serial.println("PutPacket 9");
			
			return;
		}
		
		// Возвращает тип устройства.
		L3DevType_t GetType() const
		{
			return _type;
		}
		
	protected:
		
		L3DevType_t _type;			// Тип устройства.
		packet_t _rx_packet;		// Объект принимаемого пакета.
		packet_t _tx_packet;		// Объект отправляемого пакета.
		
};

#ifdef ARDUINO_ARCH_ESP32
	#include "L3DriverBluetooth.h"
	#include "L3DriverSerial.h"
	#include "L3DriverUART.h"
#elif ARDUINO_ARCH_AVR
	#include "L3DriverSerial.h"
	#include "L3DriverSoftSerial.h"
	#include "L3DriverUART.h"
#endif
