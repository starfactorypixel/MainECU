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
	// TWAI_ALERT_RX_FIFO_OVERRUN есть не во всех версиях IDF
#ifdef TWAI_ALERT_RX_FIFO_OVERRUN
	static constexpr uint32_t ALERT_FIFO_OVERRUN = TWAI_ALERT_RX_FIFO_OVERRUN;
#else
	static constexpr uint32_t ALERT_FIFO_OVERRUN = 0;
#endif

	// TWAI_ALERT_AND_LOG не включать: печатает в лог прямо из ISR
	static constexpr uint32_t ALERTS = TWAI_ALERT_TX_FAILED | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL | ALERT_FIFO_OVERRUN | TWAI_ALERT_ABOVE_ERR_WARN | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED;

	public:
		
		L2DriverTWAI()
		{
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
			twai_stop();
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

		virtual uint32_t Poll() override
		{
			uint32_t errors = L2_ERR_NONE;

			if(_installed == false) return errors;

			uint32_t alerts = 0;
			if(twai_read_alerts(&alerts, 0) != ESP_OK)
				return errors;

			if(alerts & TWAI_ALERT_TX_FAILED)		errors |= L2_ERR_TX_FAILED;
			if(alerts & TWAI_ALERT_BUS_ERROR)		errors |= L2_ERR_BUS_ERROR;
			if(alerts & TWAI_ALERT_RX_QUEUE_FULL)	errors |= L2_ERR_RX_QUEUE_OVERFLOW;
			if(alerts & ALERT_FIFO_OVERRUN)			errors |= L2_ERR_RX_FIFO_OVERRUN;
			if(alerts & TWAI_ALERT_ABOVE_ERR_WARN)	errors |= L2_ERR_WARNING;
			if(alerts & TWAI_ALERT_ERR_PASS)		errors |= L2_ERR_PASSIVE;

			if(alerts & TWAI_ALERT_BUS_OFF)
			{
				_bus_off_count++;
				twai_initiate_recovery();
				errors |= L2_ERR_BUS_OFF;
			}

			if(alerts & TWAI_ALERT_BUS_RECOVERED)
			{
				twai_start();
				errors |= L2_EVT_BUS_RECOVERED;
			}

			return errors;
		}

		virtual bool IsRunning() const override
		{
			if(_installed == false) return false;

			twai_status_info_t info;
			if(twai_get_status_info(&info) != ESP_OK)
				return false;
			
			return (info.state == TWAI_STATE_RUNNING);
		}

		virtual void GetStatistics(L2Statistics &stats) const override
		{
			stats.bus_off_count = _bus_off_count;
			stats.state = L2_BUS_STOPPED;

			twai_status_info_t info;
			if(_installed == false || twai_get_status_info(&info) != ESP_OK)
				return;
			
			stats.rx_queue_overflow_count = info.rx_missed_count;
			stats.rx_fifo_overrun_count = info.rx_overrun_count;
			stats.tx_failed_count = info.tx_failed_count;
			stats.arb_lost_count = info.arb_lost_count;
			stats.bus_error_count = info.bus_error_count;

			stats.rx_pending = (uint16_t)info.msgs_to_rx;
			stats.tx_pending = (uint16_t)info.msgs_to_tx;
			stats.tx_error_counter = (uint16_t)info.tx_error_counter;
			stats.rx_error_counter = (uint16_t)info.rx_error_counter;

			switch(info.state)
			{
				case TWAI_STATE_RUNNING:
					stats.state = (info.tx_error_counter >= 128 || info.rx_error_counter >= 128) ? L2_BUS_PASSIVE : L2_BUS_ACTIVE;
					break;
				
				case TWAI_STATE_BUS_OFF:
					stats.state = L2_BUS_OFF;
					break;
				
				case TWAI_STATE_RECOVERING:
					stats.state = L2_BUS_RECOVERING;
					break;
				
				case TWAI_STATE_STOPPED:
				default:
					stats.state = L2_BUS_STOPPED;
					break;
			}
			
			return;
		}

	private:
		
		twai_filter_config_t _filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

		bool _installed = false;
		uint32_t _bus_off_count = 0;
};
