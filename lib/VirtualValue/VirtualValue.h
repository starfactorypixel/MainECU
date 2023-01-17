/*
    Класс хранения и обработки виртуальных параметров (пробег, потраченная энергия, время езды, ...)

    6 км\ч / 3.6 = 1,666 м\с * interval = растояние
*/

#pragma once

#include <string.h>
#include <L3Constants.h>

class VirtualValue
{
    static const uint16_t _max_id = 64;   // Максимальный ID хранимый в БД, от 0 до (_max_id - 1).

    
    public:
        
        struct db_t
        {
            uint16_t id;        // ID виртуального параметра.

            int32_t new_value;
            uint32_t new_time;

            uint32_t value;
        };

        using handler_t = void (*)(db_t obj);
        
        void RegHandler(uint16_t id, handler_t func)
        {

        }
        
    private:
        db_t _db[_max_id];
        
};
