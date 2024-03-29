#pragma once

#include <inttypes.h>

/*
	Протокол L3v2, в процессе проектирования!
	
	*Подключение*
	После установки физического подключения клиенту необходимо отправить запрос на авторизацию ( auth_init_req_t ).
	В ответ на запрос придёт пакет приглашения на авторизацию ( auth_init_resp_t ), в котором описан способ авторизации и ID устройства.
	Клиент, используя указанный способ авторизации, формирует пакет авторизации ( auth_req_t ) и отправляет его.
	В ответ придёт пакет результата авторизации ( auth_resp_t )

	*Пинг*
	С момента физического подключения система периодически будет запрашивать пинг ( ping_t ).
	При получении такого пакета необходимо немедленно ответить на него таким-же пакетом, заполнив его своими данными.

	*События*
	Большая часть общения устроена по принципу событий. На события необходимо подписываться.
	Клиенту необходимо подписаться на необходимые ему параметры ( subscribe_req_t ).
	Всё обновления будут приходить автоматически через событие ( event_t ).
	Отвечать на пакет события не требуется.

*/

namespace L3PacketTypes
{

	#pragma pack(push, 1)
	
	// Пакет Ping, Android => Main и Main => Android.
	struct ping_t
	{
		uint8_t funcID;				// 0x01
		uint32_t time;				// Uptime, мс.
	};






	// Пакет авторизации, инициализация, запрос Android => Main.
	struct auth_init_req_t
	{
		uint8_t funcID;				// 0x02
	};
	
	// Пакет авторизации, инициализация, ответ Main => Android.
	struct auth_init_resp_t
	{
		uint8_t funcID;				// 0x03
		uint8_t method;				// Способ авторизации. SHA1( sn[8] + rand[16] ) = 0x01
		uint8_t devID[6];			// Уникальный ID устройства
	};
	
	// Пакет авторизации, аутентификация, запрос Android => Main.
	struct auth_req_t
	{
		uint8_t funcID;				// 0x04
		uint8_t method;				// Способ авторизации. SHA1( sn[8] + rand[16] ) = 0x01
		uint8_t rand_str[16];		// Случайная строка, 16 байт
		uint8_t hash_str[20];		// Результат SHA1( sn[8] + rand[16] )
	};
	
	// Пакет авторизации, аутентификация, ответ Main => Android.
	struct auth_resp_t
	{
		uint8_t funcID;				// 0x05
		uint8_t code;				// Код ответа, 1 - успешно, 0 - ошибка
		uint32_t time;				// Uptime Main, мс.
	};





	// Пакет информации о блоке.
	struct dev_info_t
	{
		uint8_t funcID = 0x0F;
		uint16_t baseID;			// Базовый ID блока
		uint8_t type;				// Тип платы, 5 бит
		uint8_t hw_ver;				// Версия платы, 3 бита
		uint8_t sw_ver;				// Версия программы, 6 бит
		uint8_t proto_ver;			// Версия протокола CAN, 2 бита
		uint32_t uptime;			// Uptime блока, мс.
	};
	





	
	// 
	struct can_frame_t
	{
		uint8_t funcID;				// CAN ID функции
		uint8_t data[7];			// CAN данные
		uint8_t data_len;			// CAN длина данных
	};
	
	
	// Пакет подписки на параметр, запрос Android => Main.
	struct subscribe_req_t
	{
		uint8_t funcID;				// 0x10
		uint16_t devID;				// ID устройства на шине CAN, или виртуальное устройство, если > 2047
		uint8_t param;				// Доп. параметр подписки, 1 - Подписаться, 0 - Отписаться
	};
	
	// Пакет подписки на параметр, ответ Main => Android.
	struct subscribe_resp_t
	{
		uint8_t funcID;				// 0x11
		uint16_t devID;				// ID устройства на шине CAN, или виртуальное устройство, если > 2047
		uint8_t code;				// Код ответа, 1 - успешно, 0 - ошибка
		can_frame_t can_frame;		// Последний CAN пакет от устройства
	};
	
	
	// Пакет события, Android => Main и Main => Android.
	struct event_t
	{
		uint8_t funcID;				// 0x15
		uint16_t devID;				// ID устройства на шине CAN, или виртуальное устройство, если > 2047
		uint8_t param;				// Доп. параметр события
		can_frame_t can_frame;		// CAN пакет
	};


	// Пакет ошибки протокола L3, ответ Main => Android.
	struct error_t
	{
		uint8_t funcID;				// 0xE0
		uint8_t sourceID;			// funcID источника ошибки
		uint8_t code;				// Код ошибки
	};
	
	
	// Пакет передачи больших файлов. Инициализация, запрос.
	struct file_init_req_t
	{
		uint8_t funcID;				// 0xF0
		uint16_t devID;				// ID устройства на шине CAN, или виртуальное устройство, если > 2047
		uint8_t name[13];			// Имя файла
		uint32_t size;				// Размер файла в байтах
	};
	
	// Пакет передачи больших файлов. Инициализация, ответ.
	struct file_init_resp_t
	{
		uint8_t funcID;				// 0xF1
		uint8_t transferID;			// ID передаваемого файла
		uint32_t chankCount;		// Кол-во чанков с данными
		uint8_t chankSize;			// Размер одного чайка данных в байтах
	};
	
	// Пакет передачи больших файлов. Отправка, запрос.
	struct file_send_req_t
	{
		uint8_t funcID;				// 0xF2
		uint8_t transferID;			// ID передаваемого файла
		uint32_t chankNum;			// Номер передаваемого чанка
		uint8_t dataLen;			// Длина данных
		uint8_t data[54];			// Массив данных (Выбирается из расчёта ответа file_init_resp_t.chankSize)
	};
	
	// Пакет передачи больших файлов. Отправка, ответ.
	struct file_send_resp_t
	{
		uint8_t funcID;				// 0xF3
		uint8_t transferID;			// ID передаваемого файла
		uint32_t chankNum;			// Номер передаваемого чанка
		uint8_t code;				// Код ответа, 0 - Ошибка, 1 - Успешно и запрос следующего чанка, 2 - Успешно и окончание передачи (чанки кончились)
	};
	
	// Пакет передачи больших файлов. Завершение, запрос.
	struct file_fin_req_t
	{
		uint8_t funcID;				// 0xF4
		uint8_t transferID;			// ID передаваемого файла
		uint32_t crc32;				// Контрольная сумма CRC32 файла
	};
	
	// Пакет передачи больших файлов. Завершение, ответ.
	struct file_fin_resp_t
	{
		uint8_t funcID;				// 0xF5
		uint8_t transferID;			// ID передаваемого файла
		uint8_t code;				// Код ответа, 1 - успешно, 0 - ошибка
	};

	#pragma pack(pop)
	
}
