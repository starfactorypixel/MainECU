#pragma once
#include <inttypes.h>
#include "L3PacketConsts.hpp"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"

namespace L3C = L3PacketConsts;

// Общее состояние счётчиков одного линка (один экземпляр на драйвер, его делят Rx и Tx).
//   _tx — последний отправленный N, пишется только задачей отправки.
//   _rx — наибольший принятый и проверенный N партнёра, пишется только задачей приёма.
// Эпоха хранится снаружи (Config) и меняется только здесь: при запуске и при переносе счётчика.
class L3PacketSeq
{
	public:

		// Синхронное сохранение эпохи. true — сохранено.
		// Если не задан, считается, что Config сохранит значение сам (инвариант "сохранено
		// до первой отправки" тогда не гарантируется)
		typedef bool (*commit_t)();

		void Cfg(uint32_t *epoch, commit_t commit = nullptr)
		{
			_epoch = epoch;
			_commit = commit;
			_started = false;
			_dirty = false;

			return;
		}

		// Один раз при запуске, до первой отправки и приёма
		L3C::error_t Start()
		{
			_started = false;

			if(_epoch == nullptr) return L3C::ERROR_CFG_NO_SEQ;
			if(*_epoch >= L3C::SEQ_EPOCH_MAX) return L3C::ERROR_SEQ_EXHAUSTED;

			(*_epoch)++;
			_dirty = true;
			if(_Store() == false) return L3C::ERROR_SEQ_COMMIT;

			_tx = (uint64_t)(*_epoch) << 32;
			_SetRx(_tx);						// пакеты партнёра из прошлых загрузок отсекаются
			esp_fill_random(_random, sizeof(_random));
			_started = true;

			return L3C::ERROR_NONE;
		}

		// Задача отправки: выдать следующий N.
		// Подтягивается к _rx, чтобы следующий партнёр стартовал выше предыдущего
		L3C::error_t Next(uint64_t &n)
		{
			if(_started == false) return L3C::ERROR_SEQ_NOT_STARTED;

			uint64_t floor = _GetRx();
			if(floor > _tx) _tx = floor;

			if(_tx >= L3C::SEQ_N_MAX) return L3C::ERROR_SEQ_EXHAUSTED;
			_tx++;

			uint32_t epoch = (uint32_t)(_tx >> 32);
			if(epoch != *_epoch)
			{
				*_epoch = epoch;
				_dirty = true;
			}

			if(_dirty && _Store() == false) return L3C::ERROR_SEQ_COMMIT;	// без сохранения не отправляем

			n = _tx;

			return L3C::ERROR_NONE;
		}

		// Задача приёма: свежий ли номер (проверка до крипто)
		bool IsFresh(uint64_t n)
		{
			return n > _GetRx();
		}

		// Задача приёма: зафиксировать номер, ТОЛЬКО после успешной проверки тега
		void Accept(uint64_t n)
		{
			portENTER_CRITICAL(&_mux);
			if(n > _rx) _rx = n;
			portEXIT_CRITICAL(&_mux);

			return;
		}

		bool IsStarted() const
		{
			return _started;
		}

		const uint8_t *Random() const
		{
			return _random;
		}

	private:

		bool _Store()
		{
			if(_commit != nullptr && _commit() == false) return false;
			_dirty = false;

			return true;
		}

		uint64_t _GetRx()
		{
			portENTER_CRITICAL(&_mux);
			uint64_t v = _rx;
			portEXIT_CRITICAL(&_mux);

			return v;
		}

		void _SetRx(uint64_t v)
		{
			portENTER_CRITICAL(&_mux);
			_rx = v;
			portEXIT_CRITICAL(&_mux);

			return;
		}

		uint32_t *_epoch = nullptr;
		commit_t _commit = nullptr;
		uint64_t _tx = 0;
		uint64_t _rx = 0;
		uint8_t _random[4] = {};
		bool _started = false;
		bool _dirty = false;
		portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
