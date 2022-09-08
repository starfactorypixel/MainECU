/*
    Класс базы данных полученных параметров из шины CAN.
    Пока настроен на версию CAN 2.0A. Реализация версии 2.0B потребует использовать другой подход, с динамическими списками :(
    В данный момент: (8 + 1 + 4) * 2048 = 26624 = 26КБ SRAM памяти занимает эта БД.
*/

class StateDB
{
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
        
        bool Set(uint16_t id, uint8_t *data, uint8_t length, uint32_t time)
        {
            bool result = false;
            
            if(id < this->_max_id && length < this->_max_data)
            {
                for(uint8_t i = 0; i < length; ++i)
                {
                    this->_db[id].data[i] = data[i];
                }
                this->_db[id].length = length;
                this->_db[id].time = time;

                result = true;
            }
            
            return result;
        }
        
        bool Set(uint16_t id, db_t &obj)
        {
            bool result = false;
            
            if(id < this->_max_id && obj.length < this->_max_data)
            {
                for(uint8_t i = 0; i < obj.length; ++i)
                {
                    this->_db[id].data[i] = obj.data[i];
                }
                this->_db[id].length = obj.length;
                this->_db[id].time = obj.time;
                
                result = true;
            }
            
            return result;
        }
        
        bool Get(uint16_t id, uint8_t *&data, uint8_t &length, uint32_t &time)
        {
            bool result = false;

            if(id < this->_max_id && this->_db[id].time > 0)
            {
                data = &this->_db[id].data[0];
                length = this->_db[id].length;
                time = this->_db[id].time;

                result = true;
            }
            else
            {
                data = 0x00;
                length = 0;
            }
            
            return result;
        }

        bool Get(uint16_t id, db_t &obj)
        {
            bool result = false;
            
            if(id < this->_max_id && this->_db[id].time > 0)
            {
                obj = this->_db[id];
                
                result = true;
            }
            
            return result;
        }

    private:
        db_t _db[_max_id];

};
