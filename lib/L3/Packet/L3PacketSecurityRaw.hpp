#pragma once
#include <inttypes.h>
#include <string.h>
#include "L3PacketConsts.hpp"
#include "mbedtls/chachapoly.h"
#include "esp_random.h"

namespace L3 = L3PacketConsts;

class L3PacketSecurityRaw
{
	static constexpr L3::format_t _format = L3::FORMAT_RAW;
	
	public:

		L3PacketSecurityRaw()
		{
			sizeof(L3::security_t);
		}
		
		// Вставить пакет целиком при приёме
		bool PutPacketPtr(const uint8_t *data, uint16_t length)
		{
			if(length < L3::SECURITY_PACKET_LEN_MIN || length > L3::SECURITY_PACKET_LEN_MAX) return false;
			
			memcpy(&_packet, data, length);
			_packet_length = length;

			_Parse();
			return true;
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
			if(length > L3::SECURITY_PAYLOAD_RAW_LEN) return false;
			
			memcpy(_packet.payload_raw.data, data, length);
			_packet.payload_len = length;
			return true;
		}
		
		// Добавить payload в пакет
		// Добавляет к уже добавленным данным
		bool AddPayload(const uint8_t *data, uint16_t length)
		{
			if(length > L3::SECURITY_PAYLOAD_RAW_LEN) return false;
			if(_packet.payload_len + length > L3::SECURITY_PAYLOAD_RAW_LEN) return false;
			
			memcpy(&_packet.payload_raw.data[_packet.payload_len], data, length);
			_packet.payload_len += length;
			return true;
		}
		
		// Взять payload из пакета для анализа
		// Если пакет не готов, то вернёт 0
		uint16_t GetPayloadPtr(uint8_t *&data)
		{
			if(_param.parsed == false) return 0;
			
			data = _packet.payload_raw.data;
			return _packet.payload_len;
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
			memset(&_packet, 0x00, L3::SECURITY_PACKET_LEN_MIN);
			_param.error = L3::ERROR_NONE;
			_param.parsed = false;
			
			return;
		}
		
	private:
		
		// Подготовка пакета перед отправкой
		void _Prepare()
		{
			_packet_length = L3::SECURITY_PACKET_LEN_MIN + _packet.payload_len;
			_packet.format = _format;
			
			return;
		}
		
		// Разбор пакета при приёме
		void _Parse()
		{
			if(_packet.format != _format) return _SetError(L3::ERROR_FORMAT);
			if(_packet.payload_len > L3::SECURITY_PAYLOAD_RAW_LEN) return _SetError(L3::ERROR_LEN);
			
			_param.error = L3::ERROR_NONE;
			_param.parsed = true;
			return;
		}
		
		void _SetError(L3::error_t error)
		{
			_param.error = error;
			return;
		}
		
		L3::security_t _packet;			// Пакет
		uint16_t _packet_length;		// Фактическая длина пакета
		
		struct
		{
			uint32_t last_rx_time;
			uint16_t timeout;
			L3::error_t error;
			bool parsed;
		} _param;
};
