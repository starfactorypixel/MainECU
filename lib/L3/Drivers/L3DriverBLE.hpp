#pragma once
#include <inttypes.h>
#include <string.h>
#include "Drivers/L3Driver.hpp"
#include "esp32-hal-bt.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
Канал BLE (Bluedroid из ядра Arduino, функции IDF). Телефон — клиент, соединение одно.

Реклама (legacy, задаётся один раз):
	- пакет: флаги + manufacturer data [Company ID 0xFFFF][PACKET_HEAD_0 PACKET_HEAD_1 ('P''X')][SN 8];
	- ответ на сканирование: имя (до 29 байт, длиннее — обрезано по границе символа UTF-8);
	- нет соединения — к рекламе можно подключиться (ADV_IND), есть — нельзя (ADV_SCAN_IND):
	  устройство остаётся в списке, телефон видит «занято».
GATT: сервис Nordic UART. RX (…0002) — запись без ответа, телефон -> ESP; TX (…0003) — уведомления.
Одна запись — один кадр, одно уведомление — один кадр (до PACKET_LEN_MAX байт, нужен MTU >= PACKET_LEN_MAX + 3).
Транспорт открыт (_TransportOpened), когда телефон подключён, включил уведомления и MTU достаточный:
сразу уходит подписанный beacon. Сеанс не открылся за OPEN_TIMEOUT мс после подключения — разрыв.
Контекст: события Bluetooth — задача BTC; в L3Driver от неё только RxBytes() и флаги транспорта.
*/
class L3DriverBLE : public L3Driver
{
	static constexpr uint16_t MTU_LOCAL = 247;
	static constexpr uint16_t MTU_MIN = L3C::PACKET_LEN_MAX + 3;		// ATT: уведомление = MTU - 3
	static constexpr uint32_t OPEN_TIMEOUT = 10000;					// Подключён, а сеанса нет, мс
	static constexpr uint16_t COMPANY_ID = 0xFFFF;						// Тестовый; для серийного продукта нужен свой
	static constexpr uint8_t ADV_NAME_MAX = 29;						// 31 - [len][type]

	enum : uint8_t { IDX_SVC, IDX_RX_CHAR, IDX_RX_VAL, IDX_TX_CHAR, IDX_TX_VAL, IDX_TX_CCC, IDX_NB };

	public:
		L3DriverBLE()
		{
			_dev_type = DEVTYPE_BLUETOOTH;
			_cfg.beacon_interval = 5000;
			_cfg.idle = 1000;
			_cfg.ping_interval = 1000;
			_cfg.ping_retries = 3;
			_cfg.link = true;

			_CfgFormat(L3C::FORMAT_ENCRYPT);	// Радио: данные закрыты; beacon — всегда SIGN
		}

		virtual void Open() override
		{
			auto &cfg = Config::Obj().security.dev_02;	// ИМЕННО ссылка: и epoch, и key должны указывать в Config
			_OpenChannel(cfg.key, &cfg.epoch);

			Self() = this;
			_BuildAdvData();

			esp_timer_create_args_t timer_args = {};
			timer_args.callback = &OpenTimerCb;
			timer_args.arg = this;
			timer_args.dispatch_method = ESP_TIMER_TASK;
			timer_args.name = "L3BLE_OPEN";
			esp_timer_create(&timer_args, &_open_timer);

			// btStart() из ядра: заодно сообщает ядру, что BT используется (иначе ядро освобождает память контроллера)
			if(btStart() == false) { DEBUG_LOG_TOPIC("L3BLE", "Controller start failed\n"); return; }
			if(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED && esp_bluedroid_init() != ESP_OK) { DEBUG_LOG_TOPIC("L3BLE", "Bluedroid init failed\n"); return; }
			if(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED && esp_bluedroid_enable() != ESP_OK) { DEBUG_LOG_TOPIC("L3BLE", "Bluedroid enable failed\n"); return; }

			esp_ble_gap_register_callback(&GapEvent);
			esp_ble_gatts_register_callback(&GattsEvent);
			esp_ble_gatt_set_local_mtu(MTU_LOCAL);
			esp_ble_gatts_app_register(0);		// Дальше — ESP_GATTS_REG_EVT

			return;
		}

	protected:

		virtual void OnSessionClose(close_reason_t reason) override
		{
			if(reason == CLOSE_TIMEOUT) _Disconnect();	// Клиент перестал отвечать: освободить соединение

			return;
		}

	private:

		virtual void SendData(const uint8_t *data, uint16_t length) override
		{
			if(_ready == false) return;

			// ESP_FAIL — буферы стека заняты: короткая пауза и повтор. Не вышло — кадр теряется
			for(uint8_t i = 0; i < 10; ++i)
			{
				esp_err_t err = esp_ble_gatts_send_indicate(_gatts_if, _conn_id, _handles[IDX_TX_VAL], length, (uint8_t *)data, false);
				if(err == ESP_OK) return;
				if(err != ESP_FAIL) break;
				vTaskDelay(1);
			}
			DEBUG_LOG_TOPIC("L3BLE", "Notify dropped\n");

			return;
		}

		void _BuildAdvData()
		{
			const auto &cfg = Config::Obj().security;	// Ссылка, не копия (в копию попадал бы ключ)

			uint8_t *p = _adv;
			*p++ = 2; *p++ = 0x01; *p++ = 0x06;							// Flags: LE General Discoverable, без BR/EDR
			*p++ = 1 + 2 + 2 + sizeof(cfg.sn); *p++ = 0xFF;				// Manufacturer Specific Data
			*p++ = COMPANY_ID & 0xFF; *p++ = COMPANY_ID >> 8;
			*p++ = L3C::PACKET_HEAD_0; *p++ = L3C::PACKET_HEAD_1;			// Метка протокола: та же, что в начале кадра
			memcpy(p, cfg.sn, sizeof(cfg.sn)); p += sizeof(cfg.sn);
			_adv_len = p - _adv;

			uint8_t len = (uint8_t)strnlen((const char *)cfg.name, sizeof(cfg.name));
			uint8_t type = 0x09;											// Complete Local Name
			if(len > ADV_NAME_MAX)
			{
				len = ADV_NAME_MAX;
				while(len > 0 && (cfg.name[len] & 0xC0) == 0x80) --len;	// Не резать символ UTF-8
				type = 0x08;												// Shortened Local Name
			}
			_rsp[0] = len + 1; _rsp[1] = type;
			memcpy(&_rsp[2], cfg.name, len);
			_rsp_len = len + 2;

			return;
		}

		// Реклама по текущему состоянию: без соединения — подключаемая, с соединением — только для списка
		void _AdvStart()
		{
			esp_ble_adv_params_t params = {};
			params.adv_int_min = 0x00A0;		// 100 мс
			params.adv_int_max = 0x00F0;		// 150 мс
			params.adv_type = _connected ? ADV_TYPE_SCAN_IND : ADV_TYPE_IND;
			params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
			params.channel_map = ADV_CHNL_ALL;
			params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
			esp_ble_gap_start_advertising(&params);

			return;
		}

		void _Disconnect()
		{
			if(_connected) esp_ble_gap_disconnect(_bda);

			return;
		}

		// Готовность транспорта меняется только здесь (задача BTC)
		void _UpdateReady()
		{
			bool ready = _connected && _notify && _mtu >= MTU_MIN;
			if(ready == _ready) return;

			_ready = ready;
			if(ready) _TransportOpened(); else _TransportClosed();

			return;
		}

		void _OnGatts(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
		{
			switch(event)
			{
				case ESP_GATTS_REG_EVT:
				{
					_gatts_if = gatts_if;
					esp_ble_gap_config_adv_data_raw(_adv, _adv_len);
					esp_ble_gap_config_scan_rsp_data_raw(_rsp, _rsp_len);
					esp_ble_gatts_create_attr_tab(_db, gatts_if, IDX_NB, 0);
					break;
				}
				case ESP_GATTS_CREAT_ATTR_TAB_EVT:
				{
					if(param->add_attr_tab.status != ESP_GATT_OK || param->add_attr_tab.num_handle != IDX_NB)
					{
						DEBUG_LOG_TOPIC("L3BLE", "Attr table error 0x%02X\n", param->add_attr_tab.status);
						break;
					}
					memcpy(_handles, param->add_attr_tab.handles, sizeof(_handles));
					esp_ble_gatts_start_service(_handles[IDX_SVC]);
					break;
				}
				case ESP_GATTS_CONNECT_EVT:
				{
					_conn_id = param->connect.conn_id;
					memcpy(_bda, param->connect.remote_bda, sizeof(_bda));
					_mtu = 23;
					_notify = false;
					_connected = true;
					esp_ble_gap_set_pkt_data_len(_bda, 251);		// Длинные пакеты LL: кадр уходит за раз, если телефон умеет
					esp_timer_start_once(_open_timer, OPEN_TIMEOUT * 1000ULL);
					_AdvStart();								// Подключаемая реклама остановилась сама; дальше — только для списка
					break;
				}
				case ESP_GATTS_DISCONNECT_EVT:
				{
					esp_timer_stop(_open_timer);
					_connected = false;
					_notify = false;
					_UpdateReady();
					esp_ble_gap_stop_advertising();				// Дальше — ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: снова подключаемая
					break;
				}
				case ESP_GATTS_MTU_EVT:
				{
					_mtu = param->mtu.mtu;
					_UpdateReady();
					break;
				}
				case ESP_GATTS_WRITE_EVT:
				{
					if(param->write.is_prep) break;				// Длинная запись по частям: кадр так не приходит
					if(param->write.handle == _handles[IDX_RX_VAL])
					{
						RxBytes(param->write.value, param->write.len, (uint32_t)(esp_timer_get_time() / 1000ULL));
					}
					else if(param->write.handle == _handles[IDX_TX_CCC] && param->write.len == 2)
					{
						_notify = (param->write.value[0] & 0x01) != 0;
						_UpdateReady();
					}
					break;
				}
				default:
				{
					break;
				}
			}

			return;
		}

		void _OnGap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
		{
			switch(event)
			{
				case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
				case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
				{
					if(++_adv_set == 2) _AdvStart();				// Оба буфера заданы — старт
					break;
				}
				case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
				{
					_AdvStart();
					break;
				}
				case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
				{
					if(param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
						DEBUG_LOG_TOPIC("L3BLE", "Adv start failed 0x%02X\n", param->adv_start_cmpl.status);
					break;
				}
				default:
				{
					break;
				}
			}

			return;
		}

		// Колбэки Bluedroid — функции C без контекста: указатель на единственный BLE-драйвер
		static L3DriverBLE *&Self()
		{
			static L3DriverBLE *self = nullptr;
			return self;
		}

		static void GattsEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
		{
			if(Self() != nullptr) Self()->_OnGatts(event, gatts_if, param);
		}

		static void GapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
		{
			if(Self() != nullptr) Self()->_OnGap(event, param);
		}

		// Задача esp_timer: подключились, а сеанс так и не открылся (нет ключа, не включили уведомления, молчат)
		static void OpenTimerCb(void *arg)
		{
			L3DriverBLE *driver = static_cast<L3DriverBLE *>(arg);
			if(driver->IsSessionActive() == false) driver->_Disconnect();
		}

		// Nordic UART Service, UUID в порядке little-endian. Поля объекта, не static: в C++11 (сборка Arduino)
		// у static constexpr массивов, чей адрес берётся, нет определения без отдельной строки в .cpp
		const uint8_t _uuid_svc[16] = {0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0,0x93,0xF3,0xA3,0xB5,0x01,0x00,0x40,0x6E};
		const uint8_t _uuid_rx[16]  = {0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0,0x93,0xF3,0xA3,0xB5,0x02,0x00,0x40,0x6E};
		const uint8_t _uuid_tx[16]  = {0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0,0x93,0xF3,0xA3,0xB5,0x03,0x00,0x40,0x6E};
		const uint16_t _uuid_primary = ESP_GATT_UUID_PRI_SERVICE;
		const uint16_t _uuid_char = ESP_GATT_UUID_CHAR_DECLARE;
		const uint16_t _uuid_ccc = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
		const uint8_t _prop_rx = ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_WRITE;
		const uint8_t _prop_tx = ESP_GATT_CHAR_PROP_BIT_NOTIFY;

		uint8_t _rx_value[MTU_LOCAL - 3] = {};
		uint8_t _tx_value[1] = {};
		uint8_t _ccc_value[2] = {};

		const esp_gatts_attr_db_t _db[IDX_NB] =
		{
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16,  (uint8_t *)&_uuid_primary, ESP_GATT_PERM_READ, 16, 16, (uint8_t *)_uuid_svc}},
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16,  (uint8_t *)&_uuid_char, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&_prop_rx}},
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)_uuid_rx, ESP_GATT_PERM_WRITE, sizeof(_rx_value), 0, _rx_value}},
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16,  (uint8_t *)&_uuid_char, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&_prop_tx}},
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)_uuid_tx, ESP_GATT_PERM_READ, sizeof(_tx_value), 0, _tx_value}},
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16,  (uint8_t *)&_uuid_ccc, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(_ccc_value), sizeof(_ccc_value), _ccc_value}},
		};


		uint8_t _adv[31] = {};
		uint8_t _rsp[31] = {};
		uint8_t _adv_len = 0;
		uint8_t _rsp_len = 0;
		uint8_t _adv_set = 0;

		esp_gatt_if_t _gatts_if = ESP_GATT_IF_NONE;
		uint16_t _handles[IDX_NB] = {};
		esp_bd_addr_t _bda = {};
		esp_timer_handle_t _open_timer = nullptr;

		volatile uint16_t _conn_id = 0;
		volatile uint16_t _mtu = 23;
		volatile bool _connected = false;
		volatile bool _notify = false;
		volatile bool _ready = false;		// Пишет задача BTC, читает SendData()
};
