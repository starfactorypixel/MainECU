#pragma once
#include <inttypes.h>

#define L3_IV_LEN			12
#define L3_DATA_LEN			64
#define L3_TAG_LEN			12

#define L3_PAYLOAD_LEN		L3_IV_LEN + L3_TAG_LEN + L3_DATA_LEN



enum packet_type_t : uint8_t
{
	TYPE_PING,				// Пакет проверки доступности
	TYPE_AUTH,				// Пакет авторизации
	TYPE_PAYLOAD,			// Пакет с полезной нагрузкой в открытом виде
	TYPE_PAYLOAD_ENC,		// Пакет с полезной нагрузкой в шифрованном виде
};



struct __attribute__((packed)) transport_t
{
	uint8_t _dummy1 : 5;
	uint8_t version : 3;				// Версия протокола: 0x02

	//uint8_t packets : 4;				// Кол-во вложенных пакетов
	uint8_t _dummy2 : 2;
	uint8_t encryption : 1;				// Флаг шифровании payload
	uint8_t direction : 1;				// Флаг направления

	uint16_t sequence;					// Индекс пакета для сбора фрагментированных пакетов
	uint8_t chunk_count;				// Кол-во чанков, если пакет фрагментирован
	uint8_t chunk_index;				// Текущий индекс чанка, если пакет фрагментирован
	
	uint16_t crc;						// CRC-16/MCRF4XX
	
	uint8_t length;						// Длина payload
	union
	{
		plain_packet_t plaindata;
		encrypted_packet_t encdata;
	};
};

// Содержимое payload если шифрование отключено или после расшифровки
struct __attribute__((packed)) plain_packet_t
{
	uint8_t fid;						// FunctionID пакета
	uint8_t length;						// Длина data
	uint8_t data[L3_DATA_LEN];			// Данные
};

// Содержимое payload если шифрование включено, метод ChaCha20-Poly1305
struct __attribute__((packed)) encrypted_packet_t
{
	uint8_t iv[L3_IV_LEN];				// Инициализационный вектор
	uint8_t tag[L3_TAG_LEN];			// Аутентификационный тег
	uint8_t data[sizeof(plain_packet_t)];	// Зашифрованные данные
};





// 0x00, Проверка связи. Нужно отправить 8 случайных байт, на что придёт новые 8 байт
struct __attribute__((packed)) test_t
{
	uint8_t random[16];			// Случайная строка, 16 байт
};

// 0x01, Проверка наличия клиента. В пакетах находятся текущий uptime устройств
struct ping_t
{
	uint32_t uptime_ms;			// Uptime, мс.
};

// 0x02, Запрос Android => Main, инициализация авторизации. Пакет пустой
struct __attribute__((packed)) auth_init_req_t
{

};

// 0x03, Ответ Main => Android, инициализация авторизации. В method находится маска разрешённых алгоритмов, в devID уникальный MACID устройства
struct __attribute__((packed)) auth_init_resp_t
{
	uint8_t method;				// Способ авторизации. SHA1( sn[8] + rand[16] ) = 0x01
	uint8_t devID[6];			// Уникальный ID устройства
};

// 0x04, Запрос Android => Main, аутентификация. В method находится выбранный Android'ом алгоритм, далее уникальные данные для выбранного алгоритма
struct __attribute__((packed)) auth_req_t
{
	uint8_t method;				// Способ авторизации. SHA1( sn[8] + rand[16] ) = 0x01
	uint8_t rand_str[16];		// Случайная строка, 16 байт
	uint8_t hash_str[20];		// Результат SHA1( sn[8] + rand[16] )
};

// 0x05, Ответ Main => Android, аутентификация. code находится ответ авторизации. Если <0 то это код ошибки, Если >0 то успех
struct __attribute__((packed)) auth_resp_t
{
	int8_t code;				// Код ответа: 1: успех, 2: успех и переход на шифрование
};



void dfg()
{
	sizeof(transport_t);
	sizeof(encrypted_packet_t);
	sizeof(plain_packet_t);
}
