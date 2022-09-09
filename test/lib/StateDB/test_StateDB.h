
#include <gtest/gtest.h>

#include "../lib/StateDB/StateDB.h"

#include <list>

class test_StateDB
{
    public:

    static uint16_t GetMaxId() { return StateDB::_max_id; }
    static uint8_t GetMaxData() { return StateDB::_max_data; }
    static const StateDB::db_t& getItemAt(uint16_t idx, const StateDB& db)
    {
        return db._db[idx];
    }

    static void InitData(StateDB::db_t &record, uint8_t fillLength, uint32_t time = 0)
    {
        // заполняем всю доступную длинну
        for(uint8_t i = 0; i < test_StateDB::GetMaxData(); ++i)
        {
            record.data[i] = i;
        }
        record.length = fillLength;
        record.time = time; 
    }

    static uint8_t GetDataAt(const StateDB::db_t &record, uint8_t pos)
    {
        return record.data[pos];
    }
};

TEST (StateDB, Constants) { 
    // константы и размеры типов
    EXPECT_EQ (2048, test_StateDB::GetMaxId());
    EXPECT_EQ (8, test_StateDB::GetMaxData());
    EXPECT_GT (test_StateDB::GetMaxId(), test_StateDB::GetMaxData());
    EXPECT_EQ (sizeof(StateDB::db_t), sizeof(uint8_t) * test_StateDB::GetMaxData() + sizeof(uint8_t) + sizeof(uint32_t));
}

TEST (StateDB, Init) { 
    StateDB db;

    // все поля инициализированы нулями
    const StateDB::db_t cleanObject{};
    for (uint16_t idx = 0; idx < test_StateDB::GetMaxData(); ++idx)
    {
        EXPECT_EQ (memcmp(&test_StateDB::getItemAt(idx, db), &cleanObject, sizeof(StateDB::db_t)), 0);
    }   
}

template <typename ReadInterface, typename WriteInterface>
void TestReadWrite(ReadInterface &read, WriteInterface &write) { 
    //! То что используется test_StateDB::GetMaxData() вместо индекса - это норм, нет смысла гонять все возможные индексы

    static const uint32_t time = 10; 
    std::vector<StateDB::db_t> storage;
    storage.resize(test_StateDB::GetMaxData() + 1);

    // запись в бд
    for (auto idx = 0; idx <= test_StateDB::GetMaxData(); ++idx)
    {
        StateDB::db_t record;
        test_StateDB::InitData(record, idx, time);

        EXPECT_TRUE(write(idx, record));
        storage[idx] = record;
    }

    // чтение
    for (auto idx = 0; idx <= test_StateDB::GetMaxData(); ++idx)
    {
        StateDB::db_t db_record{};
        EXPECT_TRUE(read(idx, db_record));

        const StateDB::db_t& stored_record = storage[idx];
        EXPECT_EQ(stored_record.length, db_record.length);

        for (auto dataIdx = 0; dataIdx < test_StateDB::GetMaxData(); ++dataIdx)
        {
            // дата в пределах записанного размера
            if (dataIdx < stored_record.length)
            {

                EXPECT_EQ(test_StateDB::GetDataAt(db_record, dataIdx), test_StateDB::GetDataAt(stored_record, dataIdx));
            }
            else
            {
                EXPECT_EQ(test_StateDB::GetDataAt(db_record, dataIdx), 0);                
            }
        }
    }

    // попытка перезаписи с тем же временем
    for (auto idx = 0; idx <= test_StateDB::GetMaxData();++idx)
    {
        EXPECT_FALSE(write(idx, storage[idx]));
    }


    // попытка перезаписи с инкрементированным временем
    for (auto idx = 0; idx <= test_StateDB::GetMaxData();++idx)
    {           
        StateDB::db_t record;
        record.time = time + 1;
        EXPECT_TRUE(write(idx, record));
    }

    // обращение за пределы зарезервированного диапозона
    {
        StateDB::db_t record;
        EXPECT_FALSE(write(test_StateDB::GetMaxId() + 1, record));
        EXPECT_FALSE(read(test_StateDB::GetMaxId() + 1, record));
    }
}

TEST (StateDB, SetGetObj) {
    StateDB db;

    auto set = [&db](uint16_t id, const StateDB::db_t &obj) -> bool {
        return db.Set(id, obj);
    };

    auto get = [&db](uint16_t id, StateDB::db_t &obj) -> bool {
        return db.Get(id, obj);
    };

    TestReadWrite(get, set);
}

TEST (StateDB, SetGetDataPtr) {
    StateDB db;

    auto set = [&db](uint16_t id, const StateDB::db_t &obj) -> bool {
        return db.Set(id, obj.data, obj.length, obj.time);
    };

    auto get = [&db](uint16_t id, StateDB::db_t &obj) -> bool { 
        return db.Get(id, obj.data, obj.length, obj.time);
    };

    TestReadWrite(get, set);
}
