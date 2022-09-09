/*
    Класс базы данных полученных параметров из шины CAN.
    Пока настроен на версию CAN 2.0A. Реализация версии 2.0B потребует использовать другой подход, с динамическими списками :(
    В данный момент: (8 + 1 + 4) * 2048 = 26624 = 26КБ SRAM памяти занимает эта БД.
*/
class test_StateDB;

class StateDB
{
    friend class test_StateDB;

    static const uint16_t _max_id = 2048;   // Максимальный ID хранимый в БД, от 0 до (_max_id - 1).
    static const uint8_t _max_data = 8;     // Максимальное кол-во байт в поле данных.
    
    public:
        
        #pragma pack(push, 1)
        struct db_t
        {
            uint8_t data[_max_data];        // Байты данных, как в CAN, или нет?
            uint8_t length;                 // Полезная длина данных.
            uint32_t time;                  // Время последнего изменения данных.
        };
        #pragma pack(pop)
        
        StateDB()
        {
            memset(this->_db, 0x00, sizeof(this->_db));
            
            return;
        }
        
        bool Set(uint16_t id, const uint8_t *data, uint8_t length, uint32_t time)
        {
            bool result = false;
            
            if(id < this->_max_id && length <= this->_max_data)
            {
                auto& item = this->_db[id];
                if (time > item.time)
                {
                    for(uint8_t i = 0; i < length; ++i)
                    {
                        item.data[i] = data[i];
                    }
                    item.length = length;
                    item.time = time;

                    result = true;
                }
            }
            
            return result;
        }
        
        bool Set(uint16_t id, const db_t &obj)
        {
            bool result = false;
            
            if(id < this->_max_id && obj.length <= this->_max_data)
            {
                if (obj.time > _db[id].time)
                {
                    auto& item = this->_db[id];
                    for(uint8_t i = 0; i < obj.length; ++i)
                    {
                        item.data[i] = obj.data[i];
                    }
                    item.length = obj.length;
                    item.time = obj.time;

                    result = true;
                }
            }
            
            return result;
        }
        
        bool Get(uint16_t id, const uint8_t*& data, uint8_t &length, uint32_t &time) const
        {
            bool result = false;

            if(id < this->_max_id && this->_db[id].time)
            {
                data = this->_db[id].data;
                length = this->_db[id].length;
                time = this->_db[id].time;

                result = true;
            }
            
            return result;
        }

        bool Get(uint16_t id, db_t &obj) const
        {
            bool result = false;
            
            if(id < this->_max_id && this->_db[id].time)
            {
                obj = this->_db[id];
                
                result = true;
            }
            
            return result;
        }

    private:
        db_t _db[_max_id];

};
