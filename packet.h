/*
 *	packet.h
 *	Class data packet.
 *
 *	@author		Nikolai Tikhonov aka Dragon_Knight <dubki4132@mail.ru>, https://vk.com/globalzone_edev
 *	@licenses	GPL-3 https://opensource.org/licenses/GPL-3.0
 *	@repo		https://github.com/Dragon-Knight/DragonNET
 */



/*

1. Я предлагаю сделать один протокол на UART, RS485, Bluetooth. Ну потому что какой смысл делать разные протоколы для одной цели?
2. Т.к. у нас идея приборки и бортового компьютера превратить в опции, то нужно дать пользователю просто протокол, но в широким спектром возможностей, а так-же с облегчённым алгоритмом работы с ним.
   Поэтому предлагаю передать тип данных, чтобы простым switch-case можно было восстановить исходный тип.

Версия протокола - В случае обновления версии протокола у нас останется возможность как поддерживать старые, так и сообщить программе с какой версией работать.
Тип транспорта - В случае использования разного транспорта возможно потребуются доп. обработки, например задержки.
Флаг направления - Нужен как для отладки, чтобы на анализаторе видеть источник пакета, так и в работе как доп. бит идентификации типа запроса.
Флаг срочных данных - Данный флаг выставляет Main ECU при любом ответе если у него есть что передать в устройство не относящиеся к текущей посылке. https://t.me/c/1747672622/1188
Тип запроса - Запрос одного параметра, Запрос параметра из CAN, Обновление прошивки устройства, Команда без ответа, Ошибка ...
ID - Идентификатор параметра, настройки или пусть до устройства обновления прошивки. Возможно, если будет использоваться CAN версии B, где идентификаторы 29 бит это поле должно стать 32 битным.

*/

/*
	Пакет:
		[0]      - Стартовый байт.
		[*1]     - Конфиг байт 1.
		  |- [7]   - \
		  |- [6]   -  3 бита, Версия протокола. Отладочная версия 000, первый релиз 001.
		  |- [5]   - /
		  |- [4]   - \
		  |- [3]   -  2 бита, Тип транспорта. 00 - Raw, 01 - RS485, 10 - Bluetooth, 11 - ...
		  |- [2]   - Резерв.
		  |- [1]   - Резерв.
		  |- [0]   - Резерв.
		[*2]     - Конфиг байт 2.
		  |- [7]   - Флаг направления: 0 - Запрос (Устройство -> Main ECU), 1 - Ответ (Main ECU -> Устройство).
		  |- [6]   - Флаг срочных данных.
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






struct StarPixelHighPacket_t
{
	uint8_t start_byte;
	
	uint8_t version:3;
	uint8_t transport:2;
	byte reserve_1:3;
	
	uint8_t direction:1;
	uint8_t urgent:1;
	byte reserve_2:1;
	uint8_t type:5;
	
	uint16_t param;
	
	uint8_t length;
	
	byte data[64];
	
	uint16_t crc;
	
	uint8_t stop_byte;
} mypacket;

*/







/*

Примеры:
	Запрос \ Ответ текущие скорости, uint8_t (0x028A)
	Запрос:  [0x3C][0b00010000][0b00000001][0x028A][0x00]         [][CRC16][0x3E]
	Ответ:   [0x3C][0b00010000][0b00100001][0x028A][0x01][0x1F][CRC16][0x3E]
	
	Запрос \ Ответ температуры двигателя 2, float (0x05D)
	Запрос:  [0x3C][0b00010000][0b00000010][0x05D][0x00]          [][CRC16][0x3E]
	Ответ:   [0x3C][0b00010000][0b00100010][0x05D][0x04][0x4229bf7d][CRC16][0x3E]

*/




#pragma once

#if defined(ARDUINO_ARCH_AVR)
	#include <util/crc16.h>
#else
	#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
	#define bitSet(value, bit) ((value) |= (1UL << (bit)))
	#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
	#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
	
	inline uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data)
	{
		data ^= crc & 0xff;
		data ^= data << 4;
		
		return ((((uint16_t)data << 8) | ((crc >> 8) & 0xff)) ^ (uint8_t)(data >> 4) ^ ((uint16_t)data << 3));
	}
#endif

template <uint8_t _maxDataLength>
class StarPixelHighPacket
{
	const byte _version = 0b010;	// Версия протокола, 3 бита.
	const byte _start_byte = 0x3C;	// Стартовый байт (знак '<').
	const byte _stop_byte = 0x3E;	// Стоповый байт (знак '>').
	
	public:
		
		enum error_t : int8_t
		{
			ERROR_NONE = 0,
			ERROR_FORMAT = -1,
			ERROR_VERSION = -2,
			ERROR_CRC = -3,
			ERROR_OVERFLOW = -4
		};
		
		StarPixelHighPacket()
		{
			this->Init();
			
			return;
		}
		
		//~StarPixelHighPacket() = default;
		
		// Copy & Move constructors.
		//StarPixelHighPacket(const StarPixelHighPacket &) = default;
		//StarPixelHighPacket(StarPixelHighPacket &&) = default;
		
		// Copy & Move assigments.
		//StarPixelHighPacket& operator=(const StarPixelHighPacket &) = default;
		//StarPixelHighPacket& operator=(StarPixelHighPacket &&) = default;
		
		// Copy assigments.
		//StarPixelHighPacket& operator=(const StarPixelHighPacket &parent)
		//{
		//	for(uint8_t i = 0; i < sizeof(this->_packet); ++i)
		//	{
		//		this->_packet[i] = parent._packet[i];
		//	}
		//	//this->_putDataIndex = parent._putDataIndex;
		//	//this->_getDataIndex = parent._getDataIndex;
		//	//this->_putPacketIndex = parent._putPacketIndex;
		//	//this->_getPacketIndex = parent._getPacketIndex;
		//	//this->_packet_size = parent._packet_size;
		//	//this->_clean = parent._clean;
		//	//this->_received = parent._received;
		//	
		//	return *this;
		//}
		
		
		
		

		// Установить ID транспорта при передачи, 2 бита.
		void Transport(uint8_t id)
		{
			this->_packet[1] |= (id % 0x04) << 3;
			
			return;
		}
		
		// Получить ID транспорта при приёме, 2 бита.
		uint8_t Transport()
		{
			return ((this->_packet[1] >> 3) & 0x03);
		}
		
		
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
		bool Data1(byte data)
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
		bool GetData(byte &data)
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
		bool Data2(byte *data, uint8_t length)
		{
			bool result = false;
			
			if(length <= _maxDataLength)
			{
				for(uint8_t i = 0; i < length; ++i)
				{
					this->_packet[6 + i] = data[i];
				}
				this->_packet[5] = length;
				
				result = true;
			}
			
			return result;
		}
		
		// Получить данные, массивом.
		void Data2(byte *data)
		{
			// ...
			
			return;
		}
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		// Получить пакет, по-байтно, при передачи пакета.
		// 	byte &packet - Ссылка на байт, который нужно передать;
		// 	result - true если байт нужно передавать;
		bool GetPacketByte(byte &packet)
		{
			bool result = false;
			
			if(this->_getPacketIndex < this->_packet_size)
			{
				packet = this->_packet[this->_getPacketIndex++];
				
				result = true;
			}
			
			return result;
		}
		
		
		// Вставить пакет, по-байтно, при приёме пакета.
		// 	byte data - Байт данных;
		// 	return - true в случае успеха;
		bool PutPacketByte(byte data)
		{
			bool result = false;
			
			if(this->_putPacketIndex < sizeof(this->_packet))
			{
				this->_packet[this->_putPacketIndex++] = data;
				
				
				/*
				for(uint8_t i = 0; i < sizeof(this->_packet); ++i)
				{
					if(this->_packet[i] < 0x10) Serial.print("0");
					Serial.print(this->_packet[i], HEX);
					Serial.print(" ");
				}
				Serial.println();
				*/
				
				
				// Дошли и приняли байт с указанием длины данных.
				if(this->_putPacketIndex >= 6)
				{
					// Дошли и приняли весь пакет.
					if(this->_putPacketIndex == this->_packet[5] + 9)
					{
						this->_Check();
					}
				}
				
				//++this->_putPacketIndex;
				
				result = true;
			}
			else
			{
				this->_error = ERROR_OVERFLOW;
			}
			
			return result;
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
		
		
		// Флаг того, что пакет чистый и готов к заполнению.
		bool IsReady()
		{
			return this->_clean;
		}
		
		
		// Подготавливает пакет к отправки.
		void Prepare()
		{
			this->_packet[0] = this->_start_byte;
			this->_packet[1] |= (this->_version << 5);
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
			
			this->_putDataIndex = 0;
			this->_getDataIndex = 0;
			
			this->_putPacketIndex = 0;
			this->_getPacketIndex = 0;
			
			this->_packet_size = 0;
			
			this->_clean = true;
			this->_received = false;
			
			this->_error = ERROR_NONE;
			
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
		
		
		byte _packet[_maxDataLength + 9];	// Массив пакета данных.
		error_t _error;					// Ошибка.
		
		uint8_t _putDataIndex;			// Индекс вставки данных в массив.
		uint8_t _getDataIndex;			// Индекс извлечения данных из массива.
		
		uint8_t _putPacketIndex;		// Индекс вставки пакета в массив.
		uint8_t _getPacketIndex;		// Индекс извлечения пакета из массива.
		
		uint8_t _packet_size;			// Размер пакета ( используется при отправки данных С устройства ).
		
		bool _clean;					// Флаг того, что пакет очищен и готов.
		bool _received;					// Флаг того, что пакет получен и проверен.
};
