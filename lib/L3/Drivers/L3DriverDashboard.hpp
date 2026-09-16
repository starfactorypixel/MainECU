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
			_transport_rx.CfgTimeout(5);
		}
		
		void Init()
		{
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
			
			xTaskCreate(RxTask, "L3UART_RX", 2048, this, 10, nullptr);
			
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
				}
			}
		}
		
		void OnPacketReceived(uint16_t length)
		{
			uint8_t *data;
			if(_transport_rx.RxPtrStart(data, length, (esp_timer_get_time() / 1000ULL)) == false) return;
			uart_read_bytes(uart_num, data, length, 0);
			_transport_rx.RxPtrEnd();
			AttemptCopyTransportToSecurityPacket();
			
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
