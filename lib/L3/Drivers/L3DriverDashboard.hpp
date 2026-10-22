#pragma once
#include <inttypes.h>
#include "Drivers/L3Driver.hpp"
#include "driver/uart.h"
#include "driver/gpio.h"

class L3DriverDashboard : public L3Driver
{
	static constexpr uart_port_t uart_num = UART_NUM_0;
	static constexpr uint16_t IDLE_BYTES = 3U;

	public:
		L3DriverDashboard()
		{
			_dev_type = DEVTYPE_DASHBOARD;
			_cfg.beacon_interval = 5000;
			_cfg.idle = 1000;
			_cfg.ping_interval = 1000;
			_cfg.ping_retries = 3;

			_tx_format = L3C::FORMAT_SIGN;
			_packet_tx.CfgFormat(_tx_format);

			_packet_rx.CfgTimeout(5);
			_packet_rx.Reset();
		}
		
		virtual void Open() override
		{
			auto &cfg = Config::body.security.dev_01;	// ИМЕННО ссылка: и epoch, и key должны указывать в Config

			_seq.Cfg(&cfg.epoch);						// эпоху Config сохраняет сам, асинхронно

			_packet_tx.CfgKey(cfg.key, _dev_type, &_seq);
			_packet_rx.CfgKey(cfg.key, _dev_type, &_seq);

			L3C::error_t err = _seq.Start();
			if(err != L3C::ERROR_NONE)
				DEBUG_LOG_TOPIC("L3New", "Seq start error: %s\n", L3C::ErrorName(err));	// подписанные пакеты отправляться не будут
			
			
			const int uart_buffer_size = (1024 * 2);
			
			uart_config_t uart_config = 
			{
				.baud_rate = 921000,
				.data_bits = UART_DATA_8_BITS,
				.parity = UART_PARITY_DISABLE,
				.stop_bits = UART_STOP_BITS_1,
				.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
				.rx_flow_ctrl_thresh = 122,
				.source_clk = UART_SCLK_XTAL,
			};
			
			uart_driver_install(uart_num, uart_buffer_size, uart_buffer_size, 16, &_uart_queue, 0);
			uart_param_config(uart_num, &uart_config);
			uart_set_pin(uart_num, GPIO_NUM_44, GPIO_NUM_43, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
			uart_set_rx_timeout(uart_num, IDLE_BYTES);
			uart_set_tx_idle_num(uart_num, ((IDLE_BYTES+2) * 10));
			
			xTaskCreate(RxTask, "L3UART_RX", 4096, this, 10, nullptr);
			
			return;
		}


		
	private:
		
		static void RxTask(void *arg)
		{
			L3DriverDashboard *driver = static_cast<L3DriverDashboard *>(arg);

			uart_event_t event;
			while(true)
			{
				if(xQueueReceive(driver->_uart_queue, &event, portMAX_DELAY) == pdTRUE)
				{
					if(event.type == UART_DATA && event.timeout_flag)
					{
						driver->OnPacketReceived(event.size);
					}
					else if(event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL)
					{
						uart_flush_input(uart_num);
						xQueueReset(driver->_uart_queue);
					}
				}
			}
		}

		void OnPacketReceived(uint16_t length)
		{
			uint8_t *data;
			if(_packet_rx.RxPtrStart(data, length, (esp_timer_get_time() / 1000ULL)) == false)
			{
				uart_flush_input(uart_num);		// иначе байты останутся и склеятся со следующим кадром
				xQueueReset(_uart_queue);
				return;
			}

			int received = uart_read_bytes(uart_num, data, length, 0);
			_packet_rx.RxPtrEnd(received > 0 ? received : 0);

			return;
		}

		virtual void SendData(const uint8_t *data, uint16_t length) override
		{
			uart_wait_tx_done(uart_num, pdMS_TO_TICKS(5));
			//uart_wait_tx_idle_polling(uart_num);
			uart_write_bytes(uart_num, data, length);
			
			return;
		}
		
		QueueHandle_t _uart_queue = nullptr;
};
