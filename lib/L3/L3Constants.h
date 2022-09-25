/*
    Константны L3 протокола
*/

#pragma once

#include <string.h>

// Длина полезных данных в пакете L3.
static const uint8_t L3PacketDataSize = 64;

// Интервал отправки ping на L3 устроиства.
static const uint16_t L3DevicePingInterval = 1500;

// Кол-во неудачных запросов ping до кика L3 устроиства.
static const uint8_t L3DevicePingCount = 3;

// Бытовая маска типов L3 устройств.
enum L3DevType_t : uint8_t
{
    L3_DEVTYPE_NONE = 0b00000000,           // Не люблю нули :|
    L3_DEVTYPE_BLUETOOTH = 0b00000001,      // Устройства Bluetooth (Телефон, Планшет).
    L3_DEVTYPE_DASHBOARD = 0b00000010,      // Приборная панель.
    L3_DEVTYPE_COMPUTER = 0b00000100,       // Бортовой компьютер.
    L3_DEVTYPE_ALL = 0b11111111             // Все устройства.
};

// Состояние L3 устройства.
enum L3DevState_t : uint8_t
{
    L3_DEVSTATE_NONE,                          // Не люблю нули :|
    L3_DEVSTATE_IDLE,                          // Ожидания подключения (Handshake).
    L3_DEVSTATE_ACTIVE,                        // Активное состояние.
    L3_DEVSTATE_PING,                          // Запрос ответа от устройства.
    L3_DEVSTATE_TIMEOUT                        // Ответ от устройства не получен длительное время.
};

// Тип запроса протокола L3.
enum L3ReqType_t : uint8_t
{
    L3_REQTYPE_MIRROR = 0x00,               // Зеркало, - что пришло, то и отправим в ответ с учётом флагов направления.
    L3_REQTYPE_GETBUF = 0x01,               // Запрос параметра из буфера.
    L3_REQTYPE_GETCAN = 0x02,               // Запрос параметра из CAN.
    L3_REQTYPE_NONE03 = 0x03,               // 
    L3_REQTYPE_NONE04 = 0x04,               // 
    L3_REQTYPE_NONE05 = 0x05,               // 
    L3_REQTYPE_NONE06 = 0x06,               // 
    L3_REQTYPE_NONE07 = 0x07,               // 
    L3_REQTYPE_NONE08 = 0x08,               // 
    L3_REQTYPE_NONE09 = 0x09,               // 
    L3_REQTYPE_NONE0A = 0x0A,               // 
    L3_REQTYPE_NONE0B = 0x0B,               // 
    L3_REQTYPE_NONE0C = 0x0C,               // 
    L3_REQTYPE_NONE0D = 0x0D,               // 
    L3_REQTYPE_NONE0E = 0x0E,               // 
    L3_REQTYPE_NONE0F = 0x0F,               // 
    L3_REQTYPE_HANDSHAKE = 0x10,            // Handshake
    L3_REQTYPE_REGID = 0x11,                // Регистрация параметра на переодическую отправку.
    L3_REQTYPE_NONE12 = 0x12,               // 
    L3_REQTYPE_NONE13 = 0x13,               // 
    L3_REQTYPE_NONE14 = 0x14,               // 
    L3_REQTYPE_EVENTS = 0x15,               // События.
    L3_REQTYPE_NONE16 = 0x16,               // 
    L3_REQTYPE_NONE17 = 0x17,               // 
    L3_REQTYPE_NONE18 = 0x18,               // 
    L3_REQTYPE_NONE19 = 0x19,               // 
    L3_REQTYPE_NONE1A = 0x1A,               // 
    L3_REQTYPE_NONE1B = 0x1B,               // 
    L3_REQTYPE_NONE1C = 0x1C,               // 
    L3_REQTYPE_NONE1D = 0x1D,               // 
    L3_REQTYPE_ERROR = 0x1E,                // Ошибка.
    L3_REQTYPE_NONE1F = 0x1F                // 
};
