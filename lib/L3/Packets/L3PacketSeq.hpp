#pragma once
#include <inttypes.h>
#include "L3PacketConsts.hpp"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"

namespace L3C = L3PacketConsts;

// Счётчики одного канала: один экземпляр на драйвер, общий для Rx и Tx.
//
//   N = epoch<<32 | counter, 56 бит. Переполнение counter просто переходит в epoch.
//   _tx — последний отправленный N. Пишет только контекст отправки (runtime).
//   _rx — последний принятый N клиента. Пишут приём (Accept) и runtime (CloseSession), поэтому под _mux.
//
// Счётчики ESP и клиента независимы. Связывают их только две зеркальные операции, их вызывает драйвер:
//   SyncTx()       — tx = max(tx, rx), перед beacon: следующий клиент возьмёт N из beacon и окажется
//                    выше всего принятого;
//   CloseSession() — rx = max(rx, tx), конец сеанса: недоставленные пакеты становятся недействительны.
//
// Эпоха лежит в Config (указатель) и сохраняется им асинхронно. Здесь только меняется значение:
// +1 при запуске и новое значение, когда старшая часть _tx выросла (переполнение или SyncTx).
class L3PacketSeq
{
	public:

		void Cfg(uint32_t *epoch)
		{
			_epoch = epoch;
			_started = false;

			return;
		}

		// Один раз при запуске, до первой отправки и приёма
		L3C::error_t Start()
		{
			_started = false;

			if(_epoch == nullptr) return L3C::ERROR_CFG_NO_SEQ;
			if(*_epoch >= L3C::SEQ_EPOCH_MAX) return L3C::ERROR_SEQ_EXHAUSTED;

			(*_epoch)++;
			_tx = (uint64_t)(*_epoch) << 32;
			_SetRx(_tx);						// пакеты клиентов из прошлых запусков отсекаются
			esp_fill_random(_random, sizeof(_random));
			_started = true;

			return L3C::ERROR_NONE;
		}

		// Отправка: следующий N
		L3C::error_t NextTx(uint64_t &n)
		{
			if(_started == false) return L3C::ERROR_SEQ_NOT_STARTED;
			if(_tx >= L3C::SEQ_N_MAX) return L3C::ERROR_SEQ_EXHAUSTED;

			_tx++;

			uint32_t epoch = (uint32_t)(_tx >> 32);
			if(epoch != *_epoch) *_epoch = epoch;	// Config сохранит асинхронно

			n = _tx;

			return L3C::ERROR_NONE;
		}

		// Перед beacon (вызывает драйвер из runtime): поднять свой счётчик до принятого
		void SyncTx()
		{
			uint64_t rx = _GetRx();
			if(rx > _tx) _tx = rx;

			return;
		}

		// Приём: свежий ли номер (проверка до крипто)
		bool IsFresh(uint64_t n)
		{
			return n > _GetRx();
		}

		// Приём: зафиксировать номер, ТОЛЬКО после успешной проверки тега
		void Accept(uint64_t n)
		{
			portENTER_CRITICAL(&_mux);
			if(n > _rx) _rx = n;
			portEXIT_CRITICAL(&_mux);

			return;
		}

		// Конец сеанса (вызывает драйвер из runtime): всё, что клиент не доставил, становится недействительным
		void CloseSession()
		{
			portENTER_CRITICAL(&_mux);
			if(_tx > _rx) _rx = _tx;
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
		uint64_t _tx = 0;
		uint64_t _rx = 0;
		uint8_t _random[4] = {};
		bool _started = false;
		portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
