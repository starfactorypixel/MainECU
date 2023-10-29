/*
	Драйвера физического уровня для протокола L3.
	Работают с регистрами, прерываниями и с горячими пакета данных.
*/

#pragma once

#include <RingBuffer.h>

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
			while( ReadAvailable() > 0 )
			{
				rx_data = ReadByte();
				if( _rx_packet_hot.PutPacketByte(rx_data, time) == true )
				{
					if(_rx_packet_hot.IsReceived() == true)
					{
						_rx_packets.Write(_rx_packet_hot);
						_rx_packet_hot.Init();
					}
				}
			}
			
			// 'Типа' прерывание по передачи байта.
			if( _tx_packets.IsEmpty() == false )
			{
				_tx_packets.Read(_tx_packet_hot);
				
				// Функция write очень долгая. Если слать несколько пакетов сразу, то приходиться ждать по 14.4мс пока UART примет пакет на передачу.
				SendBytes(_tx_packet_hot.GetPacketPtr(), _tx_packet_hot.GetPacketLength());
			}
			
			return;
		}
		
		// Флаг того, что пакет можно забирать. Это или принятый пакет или пакет с ошибкой.
		bool NeedGetPacket()
		{
			packet_t tmp = _rx_packets.First();
			return (tmp.IsReceived() == true || tmp.GetError() != tmp.ERROR_NONE);
		}
		
		// Флаг того, что можно вставлять пакет для отправки.
		bool CanPutPacket()
		{
			return ( _tx_packets.IsFull() == false );
		}
		
		// Копирует принятый пакет в тот что передан по ссылке и очищает горячий пакет.
		void GetPacket(packet_t &packet)
		{
			_rx_packets.Read(packet);
			
			return;
		}
		
		// Копирует отправляемый пакет из того что передан по ссылке и подготавливает его к отправке.
		void PutPacket(packet_t &packet)
		{
			packet.Prepare();
			while( _tx_packets.IsFull() == true ) { /* Ждём... */ }
			_tx_packets.Write(packet);
			
			return;
		}
		
		// Возвращает тип устройства.
		L3DevType_t GetType() const
		{
			return _type;
		}
		
	protected:
		
		L3DevType_t _type;						// Тип устройства.
		
		packet_t _rx_packet_hot;				// Горячий объект принимаемого пакета.
		RingBuffer<16, packet_t> _rx_packets;	// Хранилище принятых пакетов.
		
		packet_t _tx_packet_hot;				// Горячий объект отправляемого пакета.
		RingBuffer<16, packet_t> _tx_packets;	// Хранилище отправляемых пакетов.
		
};

#include "L3DriverBluetooth.h"
#include "L3DriverSerial.h"
#include "L3DriverUART.h"
