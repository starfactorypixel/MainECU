#pragma once
#include <inttypes.h>
#include "L3PacketConsts.hpp"
#include "L3PacketTransport.hpp"
#include "L3PacketSecurityRaw.hpp"
#include "L3PacketPayload.hpp"

#define structpack		struct __attribute__((packed))


/*
Приём пакетов:
	0. (ISR) Приём пакетов до IDLE. Драйвер должен реализовать этот IDLE или средствами HW или самостоятельно.
	   Формирует по одному пакету.
	1. (RtOS) Полученный пакет передаётся в transport для анализа и проверки. Если целый то дальше, иначе - отбрасываем (событие ошибки L3RxTransportError + код).
	2. (RtOS) Полученный payload от transport передаётся в security для анализа и проверки. Если целый то дальше, иначе - отбрасываем (событие ошибки L3RxSecurityError + код).
	3. (RtOS) Полученный payload от security сохраняем в кольцевой буфер RX пакетов.
	4. (Runtime) Вычитываем весь кольцевой буфер RX пакетов, разбираем его на отдельные пакеты и вызываем колбэки.
	5. 

Передача пакетов:
	1. (Runtime) Код в любом месте генерирует новый пакет и добавляет его в payload пакет.
	   Если влезает, то просто добавляем, если нет, то выполняем дальше а затем добавляем в чистый payload пакет.
	2. (Runtime) Копируем payload пакет в security payload пакет.
	3. (Runtime) Копируем security пакет в transport payload пакет.
	4. (Runtime) В transport пакете у нас готовый байт массив для отправки.
	5. (Runtime) Пытаемся отправить в HW пакет, если не получается то складываем в буфер TX пакетов.
	6. (RtOS) Переодически проверяем возможность отправки пакетов из буфера и отправляем.
	7. 

*/


class ManagerInterface
{
	public:
		virtual ~ManagerInterface() = default;
		
		virtual uint16_t RxPacket(uint8_t dev_type, const uint8_t *rx, uint16_t rx_len) = 0;
};

class DriverInterface
{
	public:
		
		// Битовая маска типов L3 устройств.
		enum dev_type_t : uint8_t
		{
			DEVTYPE_NONE = 0x00,
			DEVTYPE_DASHBOARD = (1U << 0),		// Приборная панель по UART
			DEVTYPE_BLUETOOTH = (1U << 1),		// Устройство по Bluetooth
			DEVTYPE_SUBGHZ =    (1U << 6),		// Устройство по радиоканалу
			DEVTYPE_SERVER =    (1U << 7),		// Облачный сервер через LTE модем
			DEVTYPE_ALL = 0xFF					// Все устройства
		};
		
		// Состояние L3 устройства.
		enum dev_state_t : uint8_t
		{
			DEVSTATE_NONE = 0x00,
			DEVSTATE_IDLE,						// Устройство не подключено
			DEVSTATE_ACTIVE,					// Устройство подключено
			DEVSTATE_TIMEOUT,					// Устройство перестало отвечать
			DEVSTATE_AUTHERR,					// Устройство провалило проверку авторизации или подписи
		};

};


// Драйвер устройства. Каждый драйвер наследуется от этого класса, но пока будет один
class Driver : public DriverInterface
{
	
	struct state_t
	{
		dev_state_t state;			// Состояние устройства
		struct
		{
			uint32_t time;			// Время последнего полученного пинга
			uint8_t retries;		// Остаток попыток получить ответ на пинг
		} ping;
	};

	

	public:

		void Init(ManagerInterface *manager)
		{
			_manager = manager;

			return;
		}

		dev_type_t GetType()
		{
			return _dev_type;
		}

		// Добавляем программный пакет на отправку
		void AddPacket(const uint8_t *data, uint16_t length)
		{
			// Тут копируем данные в payload L3PacketSecurity до тех пор, пока они влезают.
			// Если больше не влезают, то вызываем Commit() чтобы зафиксировать набранный пакет и отправить его

			// Внутрь метода добавить проверку, что даже в пустой пакет у нас не влезают данные и возвращать отдельный код ошибки.
			// чтобы не пробовать отправлять данные больше payload.
			bool result = _security_tx.AddPayload(data, length);
			if(result == false)
			{
				// Не вставили потому что нету места, значит фиксируем пакет и отправляем его

				Commit();

				// Вставляем данные в уже новый подготовленный пакет
				_security_tx.AddPayload(data, length);
			}

		}

		void Commit()
		{
			// Говорим пакету что нужно выполнить все операции и подготовить пакет к отправке
			uint8_t *data;
			uint16_t length = _security_tx.GetPacketPtr(data);
			if(length > 0)
			{
				bool result = _transport_tx.PutPayload(data, length);
				if(result == true)
				{
					uint8_t *data2;
					uint16_t length2 = _transport_tx.GetPacketPtr(data2);
					if(length2 > 0)
					{
						SendData(data2, length2);
						// Тут у нас массив байт для отправки в нужный порт
					}
				}

				_security_tx.Init();
			}
		}

		void Reset()
		{
			memset(&_state, 0x00, sizeof(_state));
			_state.state = DEVSTATE_IDLE;
			_transport_rx.Init();
			_transport_tx.Init();
			_security_rx.Init();
			_security_tx.Init();
			_payload_rx.Init();

			return;
		}

		// Пробуем копировать Transport в Security пакет
		void AttemptCopyTransportToSecurityPacket()
		{
			// Метод GetPayloadPtr() устроен так, что он вернёт длину 0, если пакет не готов
			// Поэтому безопастно дёргать этот метод всегда чтобы убедиться что пакет собран

			uint8_t *data;
			uint16_t length = _transport_rx.GetPayloadPtr(data);
			if(length > 0)
			{
				// Тут нужно копировать в _security пакет полученный Payload и вставлять в буфер RX пакетов
				_security_rx.PutPacketPtr(data, length);
				
				uint8_t *data2;
				uint16_t length2 = _security_rx.GetPayloadPtr(data2);
				if(length2 > 0)
				{
					_payload_rx.Add(data2, length2);
				}

				_transport_rx.Init();
			}

			return;
		}


		// Каким-то образом пришёл 1 байт. Прерывание или рунтайм
		void RxByte(uint8_t rx, uint32_t rx_time)
		{
			_transport_rx.AddPacketByte(rx, rx_time);
			AttemptCopyTransportToSecurityPacket();

			return;
		}

		// Каким-то образом пришли несколько байт. Прерывание или рунтайм
		void RxBytes(const uint8_t *rx, uint16_t rx_len/*, uint32_t rx_time*/)
		{
			_transport_rx.PutPacketBytes(rx, rx_len, (esp_timer_get_time() / 1000ULL));
			AttemptCopyTransportToSecurityPacket();
		}

		void Processing(uint32_t time)
		{
			// Постоянно проверяем наличие принятых пакетов из буфера. Вытащить из буфера и выполнить.
			// Пока без буфера а 1 объект

			while(true)
			{
				uint8_t *data;
				uint16_t length = _payload_rx.Get(data);

				if(length == 0) break;
				
				uint16_t offset = 0;
				while(offset < length)
				{
					uint16_t processed = _manager->RxPacket(_dev_type, data + offset, length - offset);
					if(processed == 0)
						break;
						
					offset += processed;
				}

				_payload_rx.Delete();
			}
			
			return;
		}


	protected:
		
		L3PacketSecurityRaw _security_tx;
		L3PacketTransport _transport_tx;

		
		L3PacketTransport _transport_rx;
		L3PacketSecurityRaw _security_rx;
		L3PacketPayload _payload_rx;

		ManagerInterface *_manager;

		dev_type_t _dev_type;
		state_t _state;

	private:
		
		virtual void SendData(const uint8_t *data, uint16_t length) = 0;
};

#include "driver/uart.h"
#include "driver/gpio.h"
class DriverDashboard : public Driver
{
	static constexpr uart_port_t uart_num = UART_NUM_0;
	static constexpr uint16_t IDLE_BYTES = 3U;

	public:
		DriverDashboard()
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
			DriverDashboard *driver = static_cast<DriverDashboard *>(arg);
			
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





// Общий менеджер для отправки и приёма данных из L3 устройств
// Для каждого устроиства создаётся отдельный объект-драйвер
class Manager : public ManagerInterface
{
	public:

		Manager()
		{

		}

		void AddDriver(Driver &driver)
		{
			if(_driver_idx >= 8) return;
			
			driver.Init(this);
			uint8_t type = driver.GetType();
			_driver[__builtin_ctz(type)] = &driver;
			
			return;
		}

		// Метод вызывается когда пришёл готовый пакет.
		virtual uint16_t RxPacket(uint8_t dev_type, const uint8_t *rx, uint16_t rx_len) override
		{
			// Пришёл пакет
			// В rx_len указана максимальная длина которую можно обработать. это НЕ длина пакета, это >= длина пакета. Но хорошо бы убедиться что это ещё и не < пакета.
			
			uint8_t fId = rx[0];
			// Работа с rx

			// В конце необходимо вернуть фактическую длину пакета, которую вычислили базе fId.
			return 6;
		}

		// Добавляем программный пакет на отправку
		void AddPacket(uint8_t dev, const uint8_t *data, uint16_t length)
		{
			while(dev)
			{
				uint8_t bit = dev & -dev;
				_driver[bit]->AddPacket(data, length);
				dev &= ~bit;
			}
			
			return;
		}

		// Ручной вызов фиксации пакета L3PacketSecurity. Вызывается тогда, когда мы всё что хотели отправить - отправили и хотим выйти их функции, чтобы недобранный пакет финализировать и отправить.
		void Commit(uint8_t dev)
		{
			while(dev)
			{
				uint8_t bit = dev & -dev;
				_driver[bit]->Commit();
				dev &= ~bit;
			}
			
			return;
		}

		// Сброс устройства
		void Reset(uint8_t dev)
		{
			while(dev)
			{
				uint8_t bit = dev & -dev;
				_driver[bit]->Reset();
				dev &= ~bit;
			}
			
			return;
		}

	private:
		
		Driver *_driver[8];
		uint8_t _driver_idx;
};


Manager manager;
DriverDashboard dash;


structpack can_frame_t
{
	uint8_t fId = 0x17;		// Пакет изменения состояния устройства CAN
	uint16_t canId;			// ID устройства на шине CAN
	uint8_t data_len;		// CAN длина данных
	uint8_t data[8];		// CAN данные
};


void qwe()
{
	can_frame_t p1 = {};
	p1.canId = 0x0126;
	p1.data_len = 2;
	p1.data[0] = 0x65;
	p1.data[1] = 0xFF;
	
	can_frame_t p2 = {};
	p2.canId = 0x0127;
	p2.data_len = 2;
	p2.data[0] = 0x65;
	p2.data[1] = 0x00;

	manager.AddDriver(dash);

	manager.AddPacket(DriverInterface::DEVTYPE_DASHBOARD, (uint8_t *)&p1, 6);
	manager.AddPacket(DriverInterface::DEVTYPE_DASHBOARD, (uint8_t *)&p2, 6);
	manager.Commit(DriverInterface::DEVTYPE_DASHBOARD);
}
