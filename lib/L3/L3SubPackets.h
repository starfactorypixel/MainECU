/*
    https://docs.google.com/spreadsheets/d/1rOYpiT1S05CxcOVvX0OtkjVuEJVvVVoyuHM8ffLTHoA/edit#gid=0
*/

#pragma once

#include <stdint.h>

struct L3SubPacket
{
    /* data */
};

// Все стуктуры имет длину всех полей кроме полей данных.
// Длина полей данных равна полю length.
// Например если L3SubPacket_Services.length = 2 и в data 2 байта, то фактическая длина стуктуры = 5 байт.

// Пакет общения Main->Android и Android->Main.
// 0x10
struct L3SubPacket_Services
{
    uint16_t cmd;           // Тип команды.
    uint8_t length;         // Длина полезных данных.
    uint8_t data[64-3];     // Полезные данные.
};

// Пакет подписки на обновление параметров.
// 0x11.
struct L3SubPacket_Subscribe
{
    uint32_t id;            // ID параметра. от 0 до 2047. 0хFFFF - Все.
    uint8_t mode;           // 1 - Подписаться, 0 - Отписаться.
};

// Пакет события Main->Android и Android->Main.
// 0x15.
struct L3SubPacket_Event
{
    uint32_t id;            // ID параметра.
    uint8_t type;           // Тип запроса согластно протоколу CAN.
    uint8_t length;         // Длина полезных данных.
    uint8_t data[7];        // Полезные данные.
};
