// http://we.easyelectronics.ru/Soft/kolcevoy-bufer-na-s-dlya-mk.html
// Шаблон кольцевого буфера
// принимает два параметра:
// размер буфера - должен быть степенью двойки,
// тип элементов хранящихся в буфере, по умолчанию unsigned char
template<int SIZE, class DATA_T=unsigned char>
class RingBuffer
{
public:
// определяем псевдоним для индексов 
        typedef uint16_t INDEX_T;
private:
// память под буфер
        DATA_T _data[SIZE];
// количество чтений
        volatile INDEX_T _readCount;
// количество записей
        volatile INDEX_T _writeCount;
// маска для индексов
        static const INDEX_T _mask = SIZE - 1;
public:
// запись в буфер, возвращает true если значение записано
        inline bool Write(DATA_T value)
        {
                if(IsFull())
                        return false;
                _data[_writeCount++ & _mask] = value;
                return true;
        }
// чтение из буфера, возвращает true если значение прочитано
        inline bool Read(DATA_T &value)
        {
                if(IsEmpty())
                        return false;
                value = _data[_readCount++ & _mask];
                return true;
        }
// возвращает первый элемент из буфера, не удаляя его
        inline DATA_T First()const
        {
                return operator[](0);
        }
// возвращает последний элемент из буфера, не удаляя его
        inline DATA_T Last()const
        {
                return operator[](Count());
        }
// возвращает элемент по индексу
        inline DATA_T& operator[] (INDEX_T i)
        {
                if(IsEmpty() || i > Count())
                        return DATA_T();
                return _data[(_readCount + i) & _mask];
        }

        inline const DATA_T operator[] (INDEX_T i)const
        {
                if(IsEmpty())
                        return DATA_T();
                return _data[(_readCount + i) & _mask];
        }
// пуст ли буфер
        inline bool IsEmpty()const
        {
                return _writeCount == _readCount;
        }
// полон ли буфер
        inline bool IsFull()const
        {
                return ((INDEX_T)(_writeCount - _readCount) & (INDEX_T)~(_mask)) != 0;
        }
// количество элементов в буфере
        INDEX_T Count()const
        {
                return (_writeCount - _readCount) & _mask;
        }
// очистить буфер
        inline void Clear()
        {
                _readCount=0;
                _writeCount=0;
        }
// размер буфера
        inline unsigned Size()
        {return SIZE;}
};
