#pragma once
#include <inttypes.h>

class VirtualDeviceInterface
{
	public:
		
		enum algorithm_t : uint8_t
		{
			ALG_NONE = 0,		// Статическое значение
			ALG_STATIC,			// Всегда возвращает одно значение value
			ALG_RANDOM,			// Случайное значение в пределах диапазона
			ALG_MINMAX,			// Триггерное переключение между min и max
			ALG_MINFADEMAX,		// Плавное перемещение между min и max
		};
		
		virtual ~VirtualDeviceInterface() = default;
		
		virtual void GetValueBytes(uint8_t *bytes, uint8_t &length) const = 0;
		virtual bool UpdateValue(uint32_t current_time) = 0;
		virtual uint32_t GetID() const = 0;
};
