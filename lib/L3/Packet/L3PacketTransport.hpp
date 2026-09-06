#pragma once
#include <inttypes.h>
#include <string.h>
#include <CUtils_CRC16.h>
#include "L3PacketConsts.hpp"

namespace L3 = L3PacketConsts;

class L3PacketTransport
{

	public:
		
		L3PacketTransport()
		{
			sizeof(L3::transport_t);
		}
		
		void CfgTimeout(uint16_t timeout)
		{
			_param.timeout = timeout;
			return;
		}
		
		
		// Вставить пакет целиком при приёме
		void PutPacketBytes(const uint8_t *data, uint16_t length, uint32_t time)
		{
			if(time - _param.last_rx_time >= _param.timeout)
				Init();
			_param.last_rx_time = time;

			if(length >= L3::TRANSPORT_PACKET_LEN_MAX) return _SetError(L3::ERROR_OVERFLOW);
			if(length < L3::TRANSPORT_PACKET_LEN_MIN) return _SetError(L3::ERROR_MIN_LEN);

			memcpy(&_packet, data, length);
			_packet_put_idx = length;
			_packet_length = L3::TRANSPORT_PACKET_LEN_MIN + _packet.payload_len;

			_Parse();

			return;
		}
		
		// Вставить байт пакета при приёме
		void AddPacketByte(uint8_t data, uint32_t time)
		{
			if(time - _param.last_rx_time >= _param.timeout)
				Init();
			_param.last_rx_time = time;
			
			if(_packet_put_idx >= L3::TRANSPORT_PACKET_LEN_MAX) return _SetError(L3::ERROR_OVERFLOW);
			
			((uint8_t *)&_packet)[_packet_put_idx++] = data;
			
			if(_packet_put_idx == L3::TRANSPORT_PACKET_LEN_MIN)
			{
				if(_packet.payload_len > L3::TRANSPORT_PAYLOAD_LEN_MAX) return _SetError(L3::ERROR_PAYLOAD_LEN);
				
				_packet_length = L3::TRANSPORT_PACKET_LEN_MIN + _packet.payload_len;
			}
			
			if(_packet_put_idx == _packet_length)
			{
				_Parse();
			}

			return;
		}
		
		// Взять весь пакет целиком для отправки
		// При вызове пакет автоматически подготовится
		uint16_t GetPacketPtr(uint8_t *&data)
		{
			_Prepare();
			
			data = (uint8_t *)&_packet;
			return _packet_length;
		}
		
		// Вставить payload в пакет
		// Вставляет в начало, переписывая содержимое
		bool PutPayload(const uint8_t *data, uint16_t length)
		{
			if(length > L3::TRANSPORT_PAYLOAD_LEN_MAX) return false;
			
			memcpy(_packet.payload, data, length);
			_packet.payload_len = length;
			return true;
		}

		// Добавить payload в пакет
		// Добавляет к уже добавленным данным
		bool AddPayload(const uint8_t *data, uint16_t length)
		{
			if(length > L3::TRANSPORT_PAYLOAD_LEN_MAX) return false;
			if(_packet.payload_len + length > L3::TRANSPORT_PAYLOAD_LEN_MAX) return false;
			
			memcpy(&_packet.payload[_packet.payload_len], data, length);
			_packet.payload_len += length;
			return true;
		}
		
		// Взять payload из пакета для анализа
		// Если пакет не готов, то вернёт 0
		uint16_t GetPayloadPtr(uint8_t *&data)
		{
			if(_param.parsed == false) return 0;
			
			data = _packet.payload;
			return _packet.payload_len;
		}
		
		// Проверить факт получения целого пакета
		bool IsReceived()
		{
			return (_packet_put_idx == _packet_length);
		}
		
		// Проверить факт разбора входящего пакета
		bool IsParsed()
		{
			return _param.parsed;
		}

		// Проверить наличие ошибок
		bool IsError()
		{
			return (_param.error != L3::ERROR_NONE);
		}

		// Получить код ошибки
		uint8_t GetError()
		{
			return _param.error;
		}

		// Сбросить объект в начальное состояние
		void Init()
		{
			_packet_length = 0;
			_packet_put_idx = 0;
			//memset(&_packet, 0x00, sizeof(_packet));
			memset(&_packet, 0x00, L3::TRANSPORT_PACKET_LEN_MIN);
			_param.error = L3::ERROR_NONE;
			_param.parsed = false;
			
			return;
		}
		
	private:
		
		// Подготовка пакета перед отправкой
		void _Prepare()
		{
			_packet_length = L3::TRANSPORT_PACKET_LEN_MIN + _packet.payload_len;
			_packet.head[0] = 'P'; _packet.head[1] = 'X';
			_packet.version = L3::PACKET_VERSION;
			_packet.direction = L3::DIRECTION_FROM;
			_packet.crc = L3::CRC_DEFAULT_VALUE;
			_packet.crc = CRC16_XModem_Table((uint8_t *)&_packet, _packet_length);
			
			return;
		}
		
		// Разбор пакета при приёме
		void _Parse()
		{
			//if(_packet_length < L3::TRANSPORT_PACKET_LEN_MIN || _packet_length > L3::TRANSPORT_PACKET_LEN_MAX) return _SetError(L3::ERROR_LEN);
			if(_packet.head[0] != 'P' || _packet.head[1] != 'X') return _SetError(L3::ERROR_HEAD);
			if(_packet.version != L3::PACKET_VERSION) return _SetError(L3::ERROR_VERSION);
			if(_packet.direction != L3::DIRECTION_TO) return _SetError(L3::ERROR_DIRECTION);
			//if(_packet.length > L3::TRANSPORT_PAYLOAD_LEN_MAX) return _SetError(L3::ERROR_PAYLOAD_LEN);
			if(_CheckCRC() == false) return _SetError(L3::ERROR_CRC);
			
			_param.error = L3::ERROR_NONE;
			_param.parsed = true;
			return;
		}
		
		bool _CheckCRC()
		{
			uint16_t old_crc = _packet.crc;
			_packet.crc = L3::CRC_DEFAULT_VALUE;
			uint16_t new_crc = CRC16_XModem_Table((uint8_t *)&_packet, _packet_length);
			_packet.crc = old_crc;
			
			return (old_crc == new_crc);
		}
		
		void _SetError(L3::error_t error)
		{
			_param.error = error;
			return;
		}
		
		L3::transport_t _packet;		// Пакет
		uint16_t _packet_length;		// Фактическая длина пакета
		uint16_t _packet_put_idx;		// Смещение вставки пакета
		
		struct
		{
			uint32_t last_rx_time;
			uint16_t timeout;
			L3::error_t error;
			bool parsed;
		} _param;
};
