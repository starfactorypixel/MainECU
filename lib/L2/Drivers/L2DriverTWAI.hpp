#pragma once
#include <inttypes.h>
#include <string.h>
#include "driver/twai.h"
#include "driver/gpio.h"
#include "L2DriverInterface.hpp"

// Драйвер встроенного контроллера TWAI (ESP32 / ESP32-S3), IDF v4.4.
// Приём:   аппаратный FIFO (64 байта) -> ISR драйвера IDF -> очередь rx_queue_len кадров -> Receive().
// Отправка: Transmit() -> очередь tx_queue_len кадров -> ISR драйвера IDF сам отправляет следующий кадр.

class L2DriverTWAI : public L2DriverInterface
{
	static constexpr uint32_t ALERTS = TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_ERR_PASS | TWAI_ALERT_ERR_ACTIVE | TWAI_ALERT_RX_QUEUE_FULL;

	public:

		L2DriverTWAI()
		{
			//_filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

			return;
		}

		virtual ~L2DriverTWAI()
		{
			End();

			return;
		}

		// Аппаратный фильтр. Вызывать до Begin().
		// Формат code/mask — как у twai_filter_config_t (ID сдвинут влево, см. документацию IDF).
		void SetFilter(uint32_t code, uint32_t mask, bool single_filter = true)
		{
			_filter.acceptance_code = code;
			_filter.acceptance_mask = mask;
			_filter.single_filter = single_filter;

			return;
		}

		// ISR драйвера ставится на ядро, с которого вызван Begin(). Из setup() это ядро 1 (Wi-Fi живёт на ядре 0).
		virtual bool Begin() override
		{
			if(_installed == true) return true;

			twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_4, GPIO_NUM_5, TWAI_MODE_NORMAL);
			g_config.rx_queue_len = 128;
			g_config.tx_queue_len = 128;
			g_config.alerts_enabled = ALERTS;
			// g_config.intr_flags = ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM;	// только если в sdkconfig есть CONFIG_TWAI_ISR_IN_IRAM

			twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

			if(twai_driver_install(&g_config, &t_config, &_filter) != ESP_OK)
				return false;

			if(twai_start() != ESP_OK)
			{
				twai_driver_uninstall();
				return false;
			}

			_installed = true;

			return true;
		}

		virtual void End() override
		{
			if(_installed == false) return;

			twai_stop();			// в состоянии bus-off/recovering вернёт ошибку — не важно
			twai_driver_uninstall();
			_installed = false;

			return;
		}

		virtual bool Receive(L2Frame &frame) override
		{
			if(_installed == false) return false;

			twai_message_t msg;
			if(twai_receive(&msg, 0) != ESP_OK)
				return false;

			frame.id = msg.identifier;
			frame.extended = msg.extd;
			frame.rtr = msg.rtr;
			frame.length = (msg.data_length_code > 8) ? 8 : msg.data_length_code;
			if(frame.rtr == false)
				memcpy(frame.data, msg.data, frame.length);

			return true;
		}

		virtual bool Transmit(const L2Frame &frame) override
		{
			if(_installed == false) return false;

			twai_message_t msg = {};
			msg.identifier = frame.id;
			msg.extd = frame.extended;
			msg.rtr = frame.rtr;
			msg.data_length_code = (frame.length > 8) ? 8 : frame.length;
			if(frame.rtr == false)
				memcpy(msg.data, frame.data, msg.data_length_code);

			return (twai_transmit(&msg, 0) == ESP_OK);
		}

		// Неблокирующая обработка событий шины: восстановление после bus-off.
		virtual void Poll() override
		{
			if(_installed == false) return;

			uint32_t alerts = 0;
			if(twai_read_alerts(&alerts, 0) != ESP_OK)
				return;

			if(alerts & TWAI_ALERT_BUS_OFF)
			{
				_stats->bus_off_count++;
				twai_initiate_recovery();		// контроллер дождётся 128 × 11 рецессивных бит
			}
			if(alerts & TWAI_ALERT_BUS_RECOVERED)
			{
				twai_start();					// после восстановления драйвер в STOPPED
			}
			if(alerts & TWAI_ALERT_RX_QUEUE_FULL)
			{
				_stats->rx_drop_count++;			// loop не успевает разбирать очередь
			}

			return;
		}

		virtual bool IsRunning() const override
		{
			if(_installed == false) return false;

			twai_status_info_t info;
			if(twai_get_status_info(&info) != ESP_OK)
				return false;

			return (info.state == TWAI_STATE_RUNNING);
		}

		// Счётчики драйвера IDF: msgs_to_rx, msgs_to_tx, rx_missed_count, tx_failed_count, ошибки и т.д.
		bool GetStatus(twai_status_info_t &info) const
		{
			if(_installed == false) return false;

			return (twai_get_status_info(&info) == ESP_OK);
		}


	private:

		twai_filter_config_t _filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

		bool _installed = false;
};
