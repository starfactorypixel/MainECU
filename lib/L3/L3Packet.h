/*
	Пакет L3.
	https://wiki.starpixel.org/books/mainecu/page/protokol-l3
*/

/*

Пакет:
	[0]      - Стартовый байт.
	[*1]     - Конфиг байт 1.
	  |- [7]   - \
	  |- [6]   -  3 бита, Версия протокола. Отладочная версия 000, первый релиз 001.
	  |- [5]   - /
	  |- [4]   - Резерв.
	  |- [3]   - Резерв.
	  |- [2]   - Резерв.
	  |- [1]   - Резерв.
	  |- [0]   - Резерв.
	[*2]     - Конфиг байт 2.
	  |- [7]   - Флаг направления: 0 - Запрос (Устройство -> Main ECU), 1 - Ответ (Main ECU -> Устройство).
	  |- [6]   - Резерв.
	  |- [5]   - Резерв.
	  |- [4]   - \
	  |- [3]   -  \
	  |- [2]   -  5 бит, Тип запроса.
	  |- [1]   -  /
	  |- [0]   - /
	[*3,4]   - 2 байта, ID запрашиваемого \ настраиваемого параметра.
	[*5]     - Длинна данных.
	[*6+n]   - Данные.
	[7+n]    - CRC16 H.
	[8+n]    - CRC16 L.
	[9+n]    - Стоповый байт.
Символом '*' указаны фрагменты данных, участвующие в подсчёте CRC16.

struct L3Packet_t
{
	uint8_t start_byte;
	
	uint8_t version:3;
	byte reserve_1:5;
	
	uint8_t direction:1;
	byte reserve_2:2;
	uint8_t type:5;
	
	uint16_t param;
	
	uint8_t length;
	
	byte data[64];
	
	uint16_t crc;
	
	uint8_t stop_byte;
} packet;

*/

#pragma once

#if defined(ARDUINO_ARCH_AVR)
	#include <Arduino.h>
	#include <util/crc16.h>
#else
	#include <string.h>
	
	#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
	#define bitSet(value, bit) ((value) |= (1UL << (bit)))
	#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
	#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
	#define lowByte(w) ((uint8_t) ((w) & 0xff))
	#define highByte(w) ((uint8_t) ((w) >> 8))
	
	inline uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data)
	{
		data ^= crc & 0xff;
		data ^= data << 4;
		
		return ((((uint16_t)data << 8) | ((crc >> 8) & 0xff)) ^ (uint8_t)(data >> 4) ^ ((uint16_t)data << 3));
	}
#endif

template <uint8_t _maxDataLength = 64>
class L3Packet
{
	static const uint8_t _version = 0b000;		// Версия протокола, 3 бита.
	static const uint8_t _start_byte = 0x3C;	// Стартовый байт (знак '<').
	static const uint8_t _stop_byte = 0x3E;		// Стоповый байт (знак '>').
	
	public:
		
		enum error_t : int8_t
		{
			ERROR_NONE = 0,
			ERROR_FORMAT = -1,
			ERROR_VERSION = -2,
			ERROR_CRC = -3,
			ERROR_OVERFLOW = -4,
			ERROR_TIMEOUT = -5
		};
		
		L3Packet()
		{
			this->Init();
			
			return;
		}
		
		//~L3Packet() = default;
		
		// Copy & Move constructors.
		//L3Packet(const L3Packet &) = default;
		//L3Packet(L3Packet &&) = default;
		
		// Copy & Move assigments.
		//L3Packet& operator=(const L3Packet &) = default;
		//L3Packet& operator=(L3Packet &&) = default;
		
		// Copy assigments.
		L3Packet& operator=(const L3Packet &parent)
		{
			for(uint8_t i = 0; i < sizeof(this->_packet); ++i)
			{
				this->_packet[i] = parent._packet[i];
			}
			this->_timeout = parent._timeout;
			//this->_transport = parent._transport;
			this->_error = parent._error;
			this->_putDataIndex = parent._putDataIndex;
			this->_getDataIndex = parent._getDataIndex;
			this->_putPacketLastTime = parent._putPacketLastTime;
			this->_putPacketIndex = parent._putPacketIndex;
			this->_getPacketIndex = parent._getPacketIndex;
			this->_packet_size = parent._packet_size;
			this->_received = parent._received;
			
			return *this;
		}
		
		
		// Устанавливает время timeout получения пакета. Если с момента последнего вставленного байта пакета больше чем time, то генерируем ошибку.
		void SetTimeout(uint16_t time)
		{
			this->_timeout = time;
			
			return;
		}
		
		/*
		// Установить ID транспорта при передачи, 2 бита.
		void SetTransport(uint8_t id)
		{
			this->_transport = id % 0x04;
			//this->_packet[1] |= (id % 0x04) << 3;
			
			return;
		}
		
		// Получить ID транспорта при приёме, 2 бита.
		uint8_t GetTransport()
		{
			return ((this->_packet[1] >> 3) & 0x03);
		}
		*/
		
		// Установить направление передачи при передачи, 1 бит.
		void Direction(uint8_t bit)
		{
			bitWrite(this->_packet[2], 7, bit);
			
			return;
		}
		
		// Получить направление передачи при приёме, 1 бит.
		uint8_t Direction()
		{
			return ((this->_packet[2] >> 7) & 0x01);
		}
		
		/*
		// Установить флаг срочных данных при передачи, 1 бит.
		void Urgent(uint8_t bit)
		{
			bitWrite(this->_packet[2], 6, bit);
			
			return;
		}
		
		// Получить флаг срочных данных при приёме, 1 бит.
		uint8_t Urgent()
		{
			return ((this->_packet[2] >> 6) & 0x01);
		}
		*/
		
		// Установить тип пакета при передачи, 5 бит.
		void Type(uint8_t type)
		{
			this->_packet[2] |= (type % 0x20);
			
			return;
		}
		
		// Получить тип пакета при приёме, 5 бит.
		uint8_t Type()
		{
			return (this->_packet[2] & 0x1F);
		}
		
		
		// Установить ID параметра при передачи, 16 бит.
		void Param(uint16_t id)
		{
			this->_packet[3] = (id >> 8);
			this->_packet[4] = (id & 0xFF);
			
			return;
		}
		
		// Получить ID параметра при приёме, 16 бит.
		uint16_t Param()
		{
			return ((this->_packet[3] << 8) | this->_packet[4]);
		}
		
		
		// Вставить данные, по-байтно.
		bool PutData(uint8_t data)
		{
			bool result = false;
			
			if(this->_putDataIndex < _maxDataLength)
			{
				this->_packet[6 + this->_putDataIndex++] = data;
				this->_packet[5] = this->_putDataIndex;
				
				result = true;
			}
			
			return result;
		}
		
		// Получить данные, по-байтно.
		bool GetData(uint8_t &data)
		{
			bool result = false;
			
			if(this->_getDataIndex < this->_packet[5])
			{
				data = this->_packet[6 + this->_getDataIndex++];
				
				result = true;
			}
			
			return result;
		}
		
		
		// Вставить данные, массивом.
		bool PutData(uint8_t *data, uint8_t length)
		{
			bool result = false;
			
			if(length <= _maxDataLength)
			{
				for(uint8_t i = 0; i < length; ++i)
				{
					this->_packet[6 + this->_putDataIndex++] = data[i];
				}
				this->_packet[5] = this->_putDataIndex;
				
				result = true;
			}
			
			return result;
		}

		// Получить указатель на массив данных. Размер получить через GetDataLength().
		uint8_t *GetDataPtr()
		{
			return &this->_packet[6];
		}
		
		
		// Получить пакет, по-байтно, при передачи пакета.
		// 	uint8_t &data - Ссылка на байт, который нужно передать;
		// 	result - true если байт нужно передавать;
		bool GetPacketByte(uint8_t &data)
		{
			bool result = false;
			
			if(this->_getPacketIndex < this->_packet_size)
			{
				data = this->_packet[this->_getPacketIndex++];
				
				result = true;
			}
			
			return result;
		}
		
		
		// Вставить пакет, по-байтно, при приёме пакета.
		//  uint8_t data - Байт данных;
		//  uint32_t time - Время получения байта;
		//  return - true в случае успеха;
		bool PutPacketByte(uint8_t data, uint32_t time)
		{
			bool result = false;
			
			if(time - this->_putPacketLastTime < this->_timeout || this->_putPacketIndex == 0)
			{
				this->_putPacketLastTime = time;

				if(this->_putPacketIndex < sizeof(this->_packet))
				{
					this->_packet[this->_putPacketIndex++] = data;
					
					// Дошли и приняли байт с указанием длины данных.
					if(this->_putPacketIndex >= 6)
					{
						// Дошли и приняли весь пакет.
						if(this->_putPacketIndex == this->_packet[5] + 9)
						{
							this->_Check();
						}
					}
					
					result = true;
				}
				else
				{
					this->_error = ERROR_OVERFLOW;
				}
			}
			else
			{
				this->_error = ERROR_TIMEOUT;
			}
			
			return result;
		}
		
		// Получить указатель на массив пакета. Размер получить через GetPacketLength().
		uint8_t *GetPacketPtr()
		{
			return &this->_packet[0];
		}
		
		
		// Получить размер данных, при приёме.
		uint8_t GetDataLength()
		{
			return this->_packet[5];
		}
		
		// Получить размер пакета, при приёме.
		uint8_t GetPacketLength()
		{
			return this->_packet[5] + 9;
		}
		
		// Получить время последнего байта. Если (IsReceived() == true), то время получения пакета.
		uint32_t GetPacketTime()
		{
			return this->_putPacketLastTime;
		}
		
		// Получить код последней ошибки.
		int8_t GetError()
		{
			return this->_error;
		}
		
		
		// Флаг того, что пакет получен и проверен.
		bool IsReceived()
		{
			return this->_received;
		}
		
		// Флаг того, что пакет подготовлен к отправке.
		bool IsPrepared()
		{
			return (this->_packet_size > 0);
		}
		
		// Флаг того, что пакет чистый и готов к заполнению.
		bool IsReady()
		{
			// Нужно проверять на факт того, что пакет чистый и в него ничего не устанавливали.
			// Нету данных, типа пакета, ID.
			return true;
		}
		
		
		// Подготавливает пакет к отправки.
		void Prepare()
		{
			this->_packet[0] = this->_start_byte;
			this->_packet[1] |= (this->_version << 5);
			//this->_packet[1] |= (this->_transport) << 3;
			this->_packet[this->_packet[5] + 8] = this->_stop_byte;
			
			uint16_t crc = this->_GetCRC16();
			this->_packet[this->_packet[5] + 6] = highByte(crc);
			this->_packet[this->_packet[5] + 7] = lowByte(crc);
			
			this->_packet_size = this->_packet[5] + 9;
			
			return;
		}
		
		
		// Очищаем буфер и все переменные.
		void Init()
		{
			memset(&this->_packet, 0x00, sizeof(this->_packet));
			
			//this->_timeout не очищаем!
			this->_error = ERROR_NONE;
			this->_putDataIndex = 0;
			this->_getDataIndex = 0;
			this->_putPacketLastTime = 0;
			this->_putPacketIndex = 0;
			this->_getPacketIndex = 0;
			this->_packet_size = 0;
			this->_received = false;
			
			return;
		}
	protected:
		
	private:
		
		// Проверяет пакет на корректность ( Стартовый + Стоповый байт, Версия, CRC16 ).
		void _Check()
		{
			if(this->_packet[0] == this->_start_byte && this->_packet[this->_putPacketIndex - 1] == _stop_byte)
			{
				if( ((this->_packet[1] >> 5) & 0x07) == this->_version )
				{
					uint16_t crc = this->_GetCRC16();
					if(this->_packet[this->_putPacketIndex - 3] == highByte(crc) && this->_packet[this->_putPacketIndex - 2] == lowByte(crc))
					{
						this->_received = true;
					}
					else
					{
						this->_error = ERROR_CRC;
					}
				}
				else
				{
					this->_error = ERROR_VERSION;
				}
			}
			else
			{
				this->_error = ERROR_FORMAT;
			}
			
			return;
		}
		
		
		// Возвращает CRC16 от буфера.
		uint16_t _GetCRC16() const
		{
			uint16_t result = 0xFFFF;
			
			for(uint8_t i = 1; i < this->_packet[5] + 6; ++i)
			{
				result = _crc_ccitt_update(result, this->_packet[i]);
			}
			
			return result;
		}
		
		uint16_t _timeout;				// Время timeout получения пакета.
		//uint8_t _transport;				// Тип физического транспорта.

		uint8_t _packet[_maxDataLength + 9];	// Массив пакета данных.
		error_t _error;					// Ошибка.
		
		uint8_t _putDataIndex;			// Индекс вставки данных в массив.
		uint8_t _getDataIndex;			// Индекс извлечения данных из массива.
		
		uint32_t _putPacketLastTime;	// Время вставки последнего байта пакета.
		uint8_t _putPacketIndex;		// Индекс вставки пакета в массив.
		uint8_t _getPacketIndex;		// Индекс извлечения пакета из массива.
		
		uint8_t _packet_size;			// Размер пакета ( используется при отправки данных С устройства ).
		
		bool _received;					// Флаг того, что пакет получен и проверен.
};
