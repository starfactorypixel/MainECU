/*
    Класс базы данных подписок устройств L3 на оповещения о изменении состояния контролируемых параметров.
    В данный момент: 1 * 2048 = 2048 = 2КБ SRAM памяти занимает эта БД.

    !!! ТЕСТЫ НЕ ДЕЛАТЬ, УКУШУ (^_^) !!!
*/

#pragma once

#include <string.h>
#include <L3Constants.h>

class L3SubscriptionsDB
{
    static const uint16_t _max_id = 2048;   // Максимальный ID хранимый в БД, от 0 до (_max_id - 1).
    
    public:
        
        typedef uint8_t db_t;               // Тип (размерность) переменной с флагами устройств.
        
        /*
        enum DevType_t : db_t
        {
            TYPE_NONE = 0b00000000,         // Не люблю нули :|
            TYPE_BLUETOOTH = 0b00000001,    // Устройства Bluetooth (Телефон, Планшет).
            TYPE_DASHBOARD = 0b00000010,    // Приборная панель.
            TYPE_COMPUTER = 0b00000100,     // Бортовой компьютер.
            TYPE_ALL = 0b11111111           // Все устройства.
        };
        */
        
        L3SubscriptionsDB()
        {
            memset(&this->_db, 0x00, sizeof(this->_db));
            
            return;
        }
        
        /*
            Устанавливает отметку подписки устройства.
            > uint16_t id - ID параметра, от 0 до (_max_id - 1);
            > L3DevType_t dev - Тип устройства;
            > return - true в случае успеха;
        */
        bool Set(uint16_t id, L3DevType_t dev)
        {
            bool result = false;
            
            if(id < this->_max_id)
            {
                this->_db[id] |= dev;

                result = true;
            }
            
            return result;
        }
        
        /*
            Возвращает отметку подписки устройства.
            > uint16_t id - ID параметра, от 0 до (_max_id - 1);
            > L3DevType_t dev - Тип устройства;
            > return - true в случае успеха;
        */
        bool Get(uint16_t id, L3DevType_t dev)
        {
            bool result = false;
            
            if(id < this->_max_id)
            {
                if( (this->_db[id] & dev) == dev )
                {
                    result = true;
                }
            }
            
            return result;
        }
        
        /*
            Возвращает маску подписанных устройств.
            > uint16_t id - ID параметра, от 0 до (_max_id - 1);
            > return - Маска устройств типа L3DevType_t;
        */
        db_t GetDev(uint16_t id)
        {
            db_t result = (db_t)L3_DEVTYPE_NONE;
            
            if(id < this->_max_id)
            {
                result = this->_db[id];
            }
            
            return result;
        }
        
        /*
            Удаляет отметку подписки устройства.
            > uint16_t id - ID параметра, от 0 до (_max_id - 1);
            > L3DevType_t dev - Тип устройства;
            > return - true в случае успеха;
        */
        void Del(uint16_t id, L3DevType_t dev)
        {
            if(id < this->_max_id)
            {
                this->_db[id] &= ~dev;
            }
            
            return;
        }
        
        /*
            Удаляет отметку подписки устройства на все ID.
            > L3DevType_t dev - Тип устройства;
            > return - true в случае успеха;
        */
        void DelDev(L3DevType_t dev)
        {
            for(db_t &element : this->_db)
            {
                element &= ~dev;
            }
            
            return;
        }
        
    private:
        db_t _db[_max_id];
        
};
