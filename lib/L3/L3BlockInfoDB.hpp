#pragma once
#include <inttypes.h>
#include <string.h>
#include <L3Fid.hpp>
#include <CUtils.h>

/*
	Сбор информации о блоках системы по CAN.
	Наверх (callback_data_t) уходят целиком L3Fid::block_info_t (после каждого получения dynamic части, если есть
	static часть) и L3Fid::block_error_t (ошибки блоков и потеря блока). Тип различается по fId.

	ISR_CanRx только кладёт кадры BlockInfo (fId 0x70..0x75) в очередь. Вся логика выполняется в Processing,
	поэтому данные блоков доступны только из одного потока и не требуют синхронизации.

	Жизненный цикл блока:
	 - heartbeat или wakeup: блок в сети. Если блок только появился или перезагрузился — данные сбрасываются;
	 - пока static или dynamic части нет — запрос каждые _req_timeout, иначе запрос dynamic каждые _dynamic_period;
	 - нет heartbeat дольше _lost_timeout: блок не в сети, наверх уходит block_error_t BLOCK_ERR_CODE_LOST.

	Все колбеки обязательны.
*/

class L3BlockInfoDB
{
	typedef uint16_t can_id_t;
	typedef uint16_t tick_t;				// Время, мс. Хранятся младшие биты, разность считается по модулю

	static constexpr can_id_t _max_can_id = 2048;
	static constexpr uint8_t _block_shift = 4;				// 16 CAN ID на блок
	static constexpr uint8_t _max_block = _max_can_id >> _block_shift;
	static constexpr can_id_t _base_id_mask = (_max_can_id - 1) & ~((1 << _block_shift) - 1);	// 0x07F0

	static constexpr uint8_t _page_len = 6;					// Байт данных в одной странице block_info_x
	static constexpr tick_t _lost_timeout = 10500;			// 2 heartbeat + джиттер, мс
	static constexpr tick_t _req_timeout = 1000;			// Повтор запроса, если данных нет, мс
	static constexpr tick_t _dynamic_period = 5000;			// Период запроса dynamic части, мс
	static constexpr tick_t _req_interval = 5;				// Минимальный интервал между запросами в CAN, мс
	static constexpr uint8_t _hb_step_max = 3;				// Наибольший шаг counter heartbeat без перезагрузки: ~ _lost_timeout / период heartbeat
	static constexpr RingBufferIndex_T _frames_size = 64;	// Очередь кадров из ISR. Степень двойки, столько кадров вмещает

	// Интервалы должны быть меньше половины диапазона tick_t (запас на редкий вызов Processing).
	// Если нужны интервалы больше ~32 с — сменить tick_t на uint32_t.
	static constexpr tick_t _tick_half = (tick_t)(~(tick_t)0) / 2;
	static_assert(_lost_timeout < _tick_half, "_lost_timeout too big for tick_t");
	static_assert(_req_timeout < _tick_half, "_req_timeout too big for tick_t");
	static_assert(_dynamic_period < _tick_half, "_dynamic_period too big for tick_t");

	// Маска страниц, когда соответствующая часть block_info_x получена полностью. Не больше 8 страниц.
	static_assert(sizeof(L3Fid::block_info_t::istatic) <= _page_len * 8, "istatic too big for uint8_t page mask");
	static_assert(sizeof(L3Fid::block_info_t::idynamic) <= _page_len * 8, "idynamic too big for uint8_t page mask");
	static constexpr uint8_t _static_filled = (1 << ((sizeof(L3Fid::block_info_t::istatic) + _page_len - 1) / _page_len)) - 1;
	static constexpr uint8_t _dynamic_filled = (1 << ((sizeof(L3Fid::block_info_t::idynamic) + _page_len - 1) / _page_len)) - 1;

	// Блок может занимать несколько секций CAN ID подряд. Объект CanBlockInfo есть только в первой секции,
	// в следующих — только параметры. Число дополнительных секций (0..3) — в битах 0-1 features[0] static части.
	static constexpr uint8_t _sections_mask = 0x03;

	// Кадр CAN из ISR
	struct frame_t
	{
		can_id_t id;
		uint8_t length;
		uint8_t data[8];
	};

	struct blocks_info_t
	{
		L3Fid::block_info_t blockinfo = {};	// Данные блока, baseId задаётся в конструкторе и не меняется
		tick_t hb_time;						// Время последнего heartbeat или wakeup
		tick_t req_time;					// Время последнего запроса
		uint8_t hb_counter;					// counter из последнего heartbeat
		uint8_t rx_static;					// Маска принятых страниц static части, _static_filled — часть целая
		uint8_t rx_dynamic;					// Маска принятых страниц dynamic части, _dynamic_filled — часть целая
		bool online;						// Блок в сети
	};

	public:

		// Внутренние ошибки модуля
		enum error_t : uint8_t
		{
			ERROR_QUEUE_OVERFLOW = 0x01,	// Очередь кадров переполнена, часть кадров потеряна
		};

		// Ошибки, которые формирует сам модуль и отправляет как block_error_t. Группу согласовать с группами ошибок L2.
		enum block_error_l3_t : uint8_t
		{
			BLOCK_ERR_GROUP_L3 = 0xFF,		// Группа ошибок модуля
			BLOCK_ERR_CODE_LOST = 0x01,		// Блок пропал из сети (нет heartbeat). realId = baseId, subcode = 0
		};

		using func_cansend_t = void (*)(/*const*/ can_id_t id, /*const*/ uint8_t *data, /*const*/ uint8_t length);
		using callback_data_t = void (*)(/*const */uint8_t *data, uint8_t length);
		using callback_error_t = void (*)(can_id_t id, error_t code);

		// Объект один на систему: создание без колбеков, копирование и перемещение запрещены
		L3BlockInfoDB() = delete;
		L3BlockInfoDB(const L3BlockInfoDB &) = delete;
		L3BlockInfoDB(L3BlockInfoDB &&) = delete;
		L3BlockInfoDB &operator=(const L3BlockInfoDB &) = delete;
		L3BlockInfoDB &operator=(L3BlockInfoDB &&) = delete;

		L3BlockInfoDB(func_cansend_t can_send, callback_data_t data_callback, callback_error_t error_callback) :
			_blocks{}, _FuncCanSend(can_send), _CallbackData(data_callback), _CallbackError(error_callback)
		{
			for(uint8_t i = 0; i < _max_block; ++i)
			{
				_blocks[i].blockinfo.baseId = (can_id_t)(i << _block_shift);
			}

			return;
		}

		// Только фильтр и копирование кадра в очередь
		void ISR_CanRx(const can_id_t id, const uint8_t *data, const uint8_t length)
		{
			if(id >= _max_can_id) return;
			if(data == nullptr) return;
			if(length < 2 || length > 8) return;
			if(data[0] < 0x70 || data[0] > 0x75) return;

			frame_t frame;
			frame.id = id;
			frame.length = length;
			memcpy(frame.data, data, length);

			if(_frames.Write(frame) == false)
				_frames_lost++;

			return;
		}

		void Processing(uint32_t time)
		{
			const tick_t now = (tick_t)time;

			// Кадры, накопленные с прошлого вызова, но не больше, чем было на входе
			RingBufferIndex_T count = _frames.Count();
			frame_t frame;
			while(count-- > 0 && _frames.Read(frame) == true)
			{
				_RxFrame(frame, now);
			}

			const uint8_t frames_lost = _frames_lost;
			if(frames_lost != _frames_lost_seen)
			{
				_frames_lost_seen = frames_lost;
				_CallbackError(0, ERROR_QUEUE_OVERFLOW);
			}

			// Потеря блоков и запросы
			for(auto &block : _blocks)
			{
				if(block.online == false) continue;

				if((tick_t)(now - block.hb_time) >= _lost_timeout)
				{
					block.online = false;
					_SendLost(block);

					continue;
				}

				// Пока данных нет — повтор через _req_timeout, иначе обновление через _dynamic_period.
				// Не чаще одного блока в _req_interval: при старте или перезапуске не создаём пачку запросов.
				const bool ready = (block.rx_static == _static_filled && block.rx_dynamic == _dynamic_filled);
				if((tick_t)(now - block.req_time) < (ready ? _dynamic_period : _req_timeout)) continue;
				if((tick_t)(now - _req_time) < _req_interval) continue;

				// Сборка начинается заново: ответ на новый запрос не смешивается с остатками прошлого
				if(block.rx_static != _static_filled)
				{
					block.rx_static = 0;
					_Request(block, 0x32);
				}
				block.rx_dynamic = 0;
				_Request(block, 0x33);

				block.req_time = now;
				_req_time = now;
			}

			return;
		}

	private:

		void _RxFrame(const frame_t &frame, const tick_t now)
		{
			blocks_info_t &block = _blocks[(frame.id >> _block_shift)];

			switch(frame.data[0])
			{
				// block_wakeup_t: блок проснулся или перезагрузился. reason пока не используется.
				case 0x70:
				{
					_Alive(block, now, true);

					break;
				}

				// heartbeat_t: counter растёт на 1. Шаг 0 (повтор кадра) и до _hb_step_max (потеря кадров) — норма.
				// Иной шаг — блок перезагрузился без wakeup (counter начался заново).
				case 0x71:
				{
					const uint8_t step = frame.data[1] - block.hb_counter;
					_Alive(block, now, (step > _hb_step_max));
					block.hb_counter = frame.data[1];

					break;
				}

				// block_info_static_t
				case 0x72:
				{
					if(block.online == false) break;

					_RxPage((uint8_t *)&block.blockinfo.istatic, sizeof(block.blockinfo.istatic), block.rx_static, _static_filled, frame);

					break;
				}

				// block_info_dynamic_t: получена целиком и static часть есть — отправляем block_info_t
				case 0x73:
				{
					if(block.online == false) break;

					if(_RxPage((uint8_t *)&block.blockinfo.idynamic, sizeof(block.blockinfo.idynamic), block.rx_dynamic, _dynamic_filled, frame) == true &&
						block.rx_static == _static_filled)
					{
						_CallbackData((uint8_t *)&block.blockinfo, sizeof(block.blockinfo));
					}

					break;
				}

				// block_error_t
				case 0x75:
				{
					L3Fid::block_error_t error = {};

					if(frame.length != sizeof(error.error) + 1) break;

					error.baseId = _BaseId(frame.id);
					error.realId = frame.id;
					memcpy((uint8_t *)&error.error, &frame.data[1], sizeof(error.error));

					_CallbackData((uint8_t *)&error, sizeof(error));

					break;
				}

				// Не реализована логика сбора ошибок от конкретных ID а не от блока.

				default:
					break;
			}

			return;
		}

		// Признак жизни блока. Блок только появился или перезагрузился — данные сбрасываются и сразу запрашиваются.
		static void _Alive(blocks_info_t &block, const tick_t now, const bool reboot)
		{
			if(block.online == false || reboot == true)
			{
				block.rx_static = 0;
				block.rx_dynamic = 0;
				block.req_time = (tick_t)(now - _req_timeout);
			}

			block.online = true;
			block.hb_time = now;

			return;
		}

		// Приём страницы block_info_x. Страницы одного ответа могут прийти в любом порядке и с повторами.
		// Маска сбрасывается при запросе, поэтому собирается ответ на последний запрос. После сборки страницы
		// игнорируются до следующего запроса (повторы кадров, ответы на чужие запросы).
		// Возвращает true в момент, когда часть собрана целиком.
		static bool _RxPage(uint8_t *dst, const uint8_t dst_size, uint8_t &rx, const uint8_t filled, const frame_t &frame)
		{
			if(rx == filled) return false;
			if(frame.length < 3) return false;

			const uint8_t page_idx = frame.data[1];
			const uint8_t data_len = frame.length - 2;
			const uint16_t rel_idx = page_idx * _page_len;

			if(rel_idx + data_len > dst_size) return false;

			memcpy(dst + rel_idx, &frame.data[2], data_len);
			rx |= (1 << page_idx);

			return (rx == filled);
		}

		void _Request(const blocks_info_t &block, const uint8_t fId)
		{
			uint8_t packet[] = {fId};
			_FuncCanSend(block.blockinfo.baseId, packet, sizeof(packet));

			return;
		}

		// Базовый ID блока, которому принадлежит CAN ID. Ищем ближайший блок в сети в своей и до 3 предыдущих секциях.
		// Пока static часть этого блока не получена, число его секций неизвестно — возвращаем base своей секции.
		can_id_t _BaseId(const can_id_t id) const
		{
			const uint8_t section = (id >> _block_shift);

			for(uint8_t i = 0; i <= _sections_mask && i <= section; ++i)
			{
				const blocks_info_t &block = _blocks[section - i];
				if(block.online == false) continue;

				if(block.rx_static == _static_filled && (block.blockinfo.istatic.features[0] & _sections_mask) >= i)
					return block.blockinfo.baseId;

				break;
			}

			return (id & _base_id_mask);
		}

		void _SendLost(const blocks_info_t &block)
		{
			L3Fid::block_error_t error = {};
			error.baseId = block.blockinfo.baseId;
			error.realId = block.blockinfo.baseId;
			error.error.group = BLOCK_ERR_GROUP_L3;
			error.error.code = BLOCK_ERR_CODE_LOST;
			error.error.subcode = 0;

			_CallbackData((uint8_t *)&error, sizeof(error));

			return;
		}

		blocks_info_t _blocks[_max_block];

		RingBuffer<_frames_size, frame_t> _frames;
		volatile uint8_t _frames_lost = 0;		// Пишет ISR_CanRx: число потерянных из-за переполнения кадров
		uint8_t _frames_lost_seen = 0;			// Пишет Processing

		tick_t _req_time = 0;					// Время последнего запроса в CAN

		func_cansend_t _FuncCanSend;
		callback_data_t _CallbackData;
		callback_error_t _CallbackError;
};
