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
		  |- [7]   - Резерв.
		  |- [6]   - Резерв.
		  |- [5]   - Флаг направления: 0 - Запрос (Устройство -> Main ECU), 1 - Ответ (Main ECU -> Устройство).
		  |- [4]   - \
		  |- [3]   -  \
		  |- [2]   -  5 бит, Тип запроса.
		  |- [1]   -  /
		  |- [0]   - /
		[*2,3]   - 2 байта, ID запрашиваемого \ настраиваемого параметра.
		[*4]     - Длинна данных.
		[*5+n]   - Данные.
		[6+n]    - CRC16 H.
		[7+n]    - CRC16 L.
		[8+n]    - Стоповый байт.
	Символом '*' указаны фрагменты данных, участвующие в подсчёте CRC16.
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

// Ниже не смотреть, это огрызки от старого проекта.

#pragma once

#if defined(ARDUINO_ARCH_AVR)
	#include <util/crc16.h>
#else
	inline uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data)
	{
		data ^= crc & 0xff;
		data ^= data << 4;
		
		return ((((uint16_t)data << 8) | ((crc >> 8) & 0xff)) ^ (uint8_t)(data >> 4) ^ ((uint16_t)data << 3));
	}
#endif

template <uint8_t _maxDataLength>
class DragonNETPacket
{
	public:
		enum error_t : int8_t
		{
			ERROR_NONE = 0,
			ERROR_FORMAT = -1,
			ERROR_CRC = -2,
			ERROR_OVERFLOW = -3
		};
		
		
		DragonNETPacket()
		{
			this->Clean();
			
			return;
		}
		
		//~DragonNETPacket() = default;
		
		// Copy & Move constructors.
		//DragonNETPacket(const DragonNETPacket &) = default;
		//DragonNETPacket(DragonNETPacket &&) = default;
		
		// Copy & Move assigments.
		//DragonNETPacket& operator=(const DragonNETPacket &) = default;
		//DragonNETPacket& operator=(DragonNETPacket &&) = default;
		
		// Copy assigments.
		DragonNETPacket& operator=(const DragonNETPacket &parent)
		{
			for(uint8_t i = 0; i < sizeof(this->_packet); ++i)
			{
				this->_packet[i] = parent._packet[i];
			}
			//this->_putDataIndex = parent._putDataIndex;
			//this->_getDataIndex = parent._getDataIndex;
			//this->_putPacketIndex = parent._putPacketIndex;
			//this->_getPacketIndex = parent._getPacketIndex;
			//this->_packet_size = parent._packet_size;
			//this->_clean = parent._clean;
			//this->_received = parent._received;
			
			return *this;
		}
		
		
		void Transport(){}
		
		// Установить тип пакета.
		void Type(uint8_t type)
		{
			this->_packet[2] |= (type << 4);
			
			return;
		}
		
		// Получить тип пакета.
		uint8_t Type()
		{
			return (this->_packet[2] >> 4);
		}
		
		
		// Установить ID параметра.
		void Param(uint16_t id)
		{
			this->_packet[2] |= (id >> 8) & 0x0F;
			this->_packet[3] = (id & 0xFF);
			
			return;
		}
		
		// Получить ID параметра.
		uint16_t Param()
		{
			return (((this->_packet[2] & 0x0F) << 8) | this->_packet[3]);
		}
		
		
		
		
		
		
		

		
		void Flag(uint8_t flag, bool state)
		{
			bitWrite(this->_packet[3], flag, state);
			
			return;
		}
		
		bool Flag(uint8_t flag)
		{
			return bitRead(this->_packet[3], flag);
		}
		
		// Вставить данные, по-байтно.
		bool Data1(byte data)
		{
			bool result = false;
			
			if(this->_putDataIndex < _maxDataLength)
			{
				this->_packet[5 + this->_putDataIndex++] = data;
				this->_packet[4] = this->_putDataIndex;
				
				result = true;
			}
			
			return result;
		}
		
		// Получить данные, по-байтно.
		bool Data1(byte *data)
		{
			bool result = false;
			
			if(this->_getDataIndex < this->_packet[4])
			{
				data = this->_packet[5 + this->_getDataIndex++];
				
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
					this->_packet[5 + i] = data[i];
				}
				this->_packet[4] = length;
				
				result = true;
			}
			
			return result;
		}
		
		// Получить данные, массивом.
		void Data2(byte *data) const
		{
			//data = this->_packet.data;
			
			return;
		}
		
		/*
			Вставить пакет, по-байтно.
				byte data - Байт данных;
				bool control - Флаг что байт контрольный (стартовый или стоповый);
				return - true в случае успеха;
		*/
		void PutPacketByte(byte data, bool control)
		{
			if(control == true && data == this->_start_byte)
			{
				this->Clean();
			}
			
			if(this->_putPacketIndex < sizeof(this->_packet))
			{
				this->_packet[this->_putPacketIndex++] = data;
			}
			else
			{
				this->_error = ERROR_OVERFLOW;
			}
			
			if(control == true && data == this->_stop_byte)
			{
				this->_Check();
			}
			
			return;
		}
		
		/*
			Получить пакет, по-байтно.
				byte &packet - Ссылка на байт, котторый нужно передать;
				bool &control - Флаг, что байт управляющий;
				result - true если байт нужно передавать;
		*/
		bool GetPacketByte(byte &packet, bool &control)
		{
			bool result = false;
			
			if(this->_getPacketIndex < this->_packet_size)
			{
				packet = this->_packet[this->_getPacketIndex++];
				control = (this->_getPacketIndex == 1 || this->_getPacketIndex == this->_packet_size) ? true : false;
				
				result = true;
			}
			
			return result;
		}
		
		/*
			Получить размер данных.
		*/
		uint8_t GetDataLength() const
		{
			return this->_packet[4];
		}
		
		/*
			Получить размер пакета.
		*/
		uint8_t GetPacketLength() const
		{
			return this->_packet[4] + 8;
		}
		
		/*
			Флаг того, что пакет получен и проверен.
		*/
		bool IsReceived(int8_t &error) const
		{
			error = this->_error;
			
			return this->_received;
		}
		
		/*
			Флаг того, что пакет чистый и готов к заполнению.
		*/
		bool IsReady() const
		{
			return this->_clean;
		}
		
		/*
			Подготавливает пакет к отправки.
		*/
		void Prepare()
		{
			uint16_t crc = this->_GetCRC16();
			
			this->_packet[0] = this->_start_byte;
			this->_packet[this->_packet[4] + 5] = highByte(crc);
			this->_packet[this->_packet[4] + 6] = lowByte(crc);
			this->_packet[this->_packet[4] + 7] = this->_stop_byte;
			
			this->_packet_size = this->_packet[4] + 8;
			
			return;
		}
		
		/*
			Очищаем буфер и все переменные.
		*/
		void Clean()
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
		/*
			Проверяет пакет на корректность ( Стартовый + Стоповый байт, CRC16 ).
		*/
		void _Check()
		{
			if(this->_packet[0] == this->_start_byte && this->_packet[this->_putPacketIndex - 1] == _stop_byte)
			{
				uint16_t crc = this->_GetCRC16();
				if(this->_packet[this->_putPacketIndex - 3] == highByte(crc) && this->_packet[this->_putPacketIndex - 2] == lowByte(crc))
				{
					this->_received = true;
				}
				else // Ошибка CRC16.
				{
					this->_error = ERROR_CRC;
				}
			}
			else // Ошибка кадра, нету стартового или стопового байта.
			{
				this->_error = ERROR_FORMAT;
			}
			
			return;
		}
		
		/*
			Возвращает CRC16 от буфера.
		*/
		uint16_t _GetCRC16() const
		{
			uint16_t result = 0xFFFF;
			
			for(uint8_t i = 1; i < this->_packet[4] + 5; ++i)
			{
				result = _crc_ccitt_update(result, this->_packet[i]);
			}
			
			return result;
		}
		
		
		const byte _start_byte = 0x3C;	// Стартовый байт (знак '<').
		const byte _stop_byte = 0x3E;	// Стоповый байт (знак '>').
		
		// Массив пакета данных.
		byte _packet[_maxDataLength + 8];
		
		uint8_t _putDataIndex;			// Индекс вставки данных в массив.
		uint8_t _getDataIndex;			// Индекс извлечения данных из массива.
		
		uint8_t _putPacketIndex;		// Индекс вставки пакета в массив.
		uint8_t _getPacketIndex;		// Индекс извлечения пакета из массива.
		
		uint8_t _packet_size;			// Размер пакета ( используется при отправки данных С устройства ).
		
		// нужно реализовать при изменении любого поля.
		bool _clean;		// Флаг того, что пакет очищен и готов.
		bool _received;		// Флаг того, что пакет получен и проверен.
		
		error_t _error;
};
