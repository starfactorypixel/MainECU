#pragma once
#include <inttypes.h>
#include <string.h>
#include "L3PacketConsts.hpp"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"

namespace L3C = L3PacketConsts;

// Счётчики и идентификатор сеанса одного канала: один экземпляр на драйвер, общий для Rx и Tx.
//
//   N = epoch<<32 | counter, 56 бит. Переполнение counter просто переходит в epoch.
//   _tx      — последний отправленный N. Пишет только контекст отправки (runtime).
//   _rx      — последний принятый N клиента. Пишут приём (Accept) и runtime (SyncTx читает), поэтому под _mux.
//   _session — идентификатор сеанса, 4 случайных байта. Идёт в IV пакетов ESP (поле random) и в конец AAD
//              пакетов клиента. Пакет клиента, подписанный под другой идентификатор (прошлый запуск ESP,
//              закрытый сеанс), не пройдёт проверку подписи.
//
// Счётчики ESP и клиента независимы. Связывает их только SyncTx() перед beacon: tx = max(tx, rx),
// чтобы клиент, взявший N из beacon, оказался выше всего уже принятого.
//
// Новый идентификатор сеанса (NewSession) выдаёт драйвер: при запуске, при закрытии сеанса и на каждом
// beacon, пока сеанса нет. Пока сеанс открыт, идентификатор не меняется.
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
			_SetRx(_tx);						// порог приёма нового запуска; пакеты прошлых запусков
												// отсекает подпись: в их AAD другой идентификатор сеанса
			NewSession();
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

		// Новый идентификатор сеанса (вызывает драйвер из runtime). Все пакеты клиента, подписанные
		// под прежний идентификатор, с этого момента не проходят проверку подписи
		void NewSession()
		{
			uint32_t id;
			esp_fill_random(&id, sizeof(id));
			_session = id;						// 32-битная выровненная запись атомарна: задача приёма
												// увидит либо старый, либо новый идентификатор целиком
			return;
		}

		// Текущий идентификатор сеанса, 4 байта (копия, без разрыва при одновременной смене)
		void Session(uint8_t out[L3C::PACKET_AAD_BIND_LEN]) const
		{
			uint32_t id = _session;
			memcpy(out, &id, sizeof(id));

			return;
		}

		bool IsStarted() const
		{
			return _started;
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
		volatile uint32_t _session = 0;
		bool _started = false;
		portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
