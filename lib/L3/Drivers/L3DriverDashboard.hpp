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

			_CfgFormat(L3C::FORMAT_SIGN);		// UART: всегда подпись (и приём, и передача)

			_packet_rx.CfgTimeout(5);
			_packet_rx.Reset();
		}
		
		virtual void Open() override
		{
			auto &cfg = Config::body.security.dev_01;	// ИМЕННО ссылка: и epoch, и key должны указывать в Config
			_OpenChannel(cfg.key, &cfg.epoch);			// ошибка уйдёт в колбэк error

			const int uart_buffer_size = (1024 * 2);
			
			uart_config_t uart_config = 
			{
				.baud_rate = 921600,
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
		
		// Событие UART_DATA (по таймауту линии или по заполнению FIFO) — сигнал «в буфере есть данные».
		// Границы кусков с границами кадров не совпадают, кадры собирает L3PacketRx::PutStream()
		static void RxTask(void *arg)
		{
			L3DriverDashboard *driver = static_cast<L3DriverDashboard *>(arg);

			uart_event_t event;
			while(true)
			{
				if(xQueueReceive(driver->_uart_queue, &event, portMAX_DELAY) == pdTRUE)
				{
					if(event.type == UART_DATA)
					{
						driver->OnDataReceived();
					}
					else if(event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL)
					{
						uart_flush_input(uart_num);
						xQueueReset(driver->_uart_queue);
						driver->_packet_rx.DropFrame();		// начало кадра было, продолжение потеряно
					}
				}
			}
		}

		// Забрать всё, что лежит в буфере драйвера UART. Размер из события не используется:
		// если событие потерялось (очередь переполнилась), его байты не застрянут в буфере
		void OnDataReceived()
		{
			uint8_t buffer[128];
			uint32_t time = (uint32_t)(esp_timer_get_time() / 1000ULL);
			int length;

			while((length = uart_read_bytes(uart_num, buffer, sizeof(buffer), 0)) > 0)
				_packet_rx.PutStream(buffer, (uint16_t)length, time);

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
