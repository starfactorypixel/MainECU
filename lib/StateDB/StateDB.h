/*
    Класс базы данных полученных параметров из шины CAN.
    Пока настроен на версию CAN 2.0A. Реализация версии 2.0B потребует использовать другой подход, с динамическими списками :(
    В данный момент: (8 + 1 + 4) * 2048 = 26624 = 26КБ SRAM памяти занимает эта БД.
*/

template <uint16_t _max_id_count = 2048>
class StateDB
{
    public:
        
        #pragma pack(push, 1)
        struct db_t
        {
            uint8_t data[8];        // 8 байт данных, как в CAN.
            uint8_t length;         // Полезная длина данных.
            uint32_t time;          // Время последнего изменения данных.
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
            
            if(id < _max_id_count)
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
            
            if(id < _max_id_count)
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

            if(id < _max_id_count && this->_db[id].time > 0)
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
            
            if(id < _max_id_count && this->_db[id].time > 0)
            {
                obj = this->_db[id];
                
                result = true;
            }
            
            return result;
        }

    private:
        db_t _db[_max_id_count];

};
