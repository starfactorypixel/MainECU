//#include "soc/rtc_wdt.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"

#include <Arduino.h>

#include <LoggerLibrary.h>
#include "About.h"
#include "Leds.h"
#include "Config.h"

#include <StateDB.h>
#include <L2Manager.hpp>
#include <Drivers/L2DriverTWAI.hpp>
#include "CANCore.h"
#include <L3Manager.hpp>
#include <Drivers/L3DriverDashboard.hpp>
#include <Drivers/L3DriverBLE.hpp>
#include <L3SubscriptionDB.h>
#include <L3BlockInfoDB.hpp>

#include "SPICore.h"
#include "Analog.h"
#include "ScriptLogic.h"







uint32_t MyMillis()
{
	return (esp_timer_get_time() / 1000ULL);
}



void L2OnRx(const L2Frame &frame, uint32_t time);
void L2OnError(uint32_t errors, const L2Statistics &stats);

const L2Manager::callbacks_t l2_callbacks = {&L2OnRx, &L2OnError};
L2Manager L2ManagerObj(l2_callbacks, &MyMillis);
L2DriverTWAI L2TWAIObj;



void L3OnOpen(L3Driver *driver);
void L3OnClose(L3Driver *driver, L3DriverInterface::close_reason_t reason);
void L3OnRx(L3Driver *driver, const uint8_t *msg, uint16_t length);
void L3OnError(L3Driver *driver, L3C::error_t error);

const L3Manager::callbacks_t l3_callbacks = {&L3OnOpen, &L3OnClose, &L3OnRx, &L3OnError};
L3Manager L3ManagerObj(l3_callbacks, &MyMillis);		// убрать MyMillis
L3DriverDashboard L3DashboardObj;
L3DriverBLE L3DriverBLEObj;



StateDB DB;
L3SubscriptionDB SubsDB;



void OnBlockCanSend(uint16_t id, uint8_t *data, uint8_t length);
void OnBlockData(uint8_t *data, uint8_t length);
void OnBlockError(uint16_t id, L3BlockInfoDB::error_t code);

L3BlockInfoDB BlockInfoDB(&OnBlockCanSend, &OnBlockData, &OnBlockError);



#if defined(USE_EMULATOR)
#include <Emulator.h>
void EmulatorOnUpdate(uint32_t id, uint8_t *bytes, uint8_t length, uint32_t time)
{
	DB.Set(id, bytes, length, time);
	
	return;
}
Emulator<64> emulator(EmulatorOnUpdate);
VirtualDeviceInterface *emulator_objects[] = 
{
	// new VirtualDevice<type>(id, 		fId, min, max, interval, step, value, algorithm),
	new VirtualDevice<uint16_t>(0x010E, 0x61, 0, 550, 250, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x010F, 0x61, 0, 550, 250, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x0110, 0x61, 0, 600, 250, 10, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x0111, 0x61, 0, 600, 250, 10, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x0112, 0x61, 740, 840, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x0113, 0x61, 740, 840, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0114, 0x61, 0, 599, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0115, 0x61, 0, 599, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0116, 0x61, 0, 3899, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0117, 0x61, 0, 3899, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x011A, 0x61, -49, 70, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x011B, 0x61, -49, 70, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x011C, 0x61, -49, 70, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x011D, 0x61, -49, 70, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint32_t>(0x011E, 0x61, 0, 999999, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x013E, 0x61, 0, 550, 250, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x013F, 0x61, 0, 550, 250, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x0140, 0x61, 0, 600, 250, 10, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x0141, 0x61, 0, 600, 250, 10, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x0142, 0x61, 740, 840, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x0143, 0x61, 740, 840, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0144, 0x61, 0, 599, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0145, 0x61, 0, 599, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0146, 0x61, 0, 3899, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0147, 0x61, 0, 3899, 500, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x014A, 0x61, -49, 70, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x014B, 0x61, -49, 70, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x014C, 0x61, -49, 70, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x014D, 0x61, -49, 70, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint32_t>(0x014E, 0x61, 0, 999999, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),


	new VirtualDevice<int16_t>(0x0184, 0x61, -1000, 1000, 1000, 1, 1, VirtualDeviceInterface::ALG_RANDOM),
	new VirtualDevice<int16_t>(0x0185, 0x61, -1000, 1000, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),

	new VirtualDevice<uint8_t>(0x0186, 0x61, 58, 100, 10000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x0187, 0x61, 58, 100, 10000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0188, 0x61, -12100, 24200, 250, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int16_t>(0x0189, 0x61, -12100, 24200, 250, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x018C, 0x61, 740, 840, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint16_t>(0x018D, 0x61, 740, 840, 1000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int8_t>(0x0192, 0x61, -128, 59, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<int8_t>(0x0193, 0x61, -128, 59, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01C4, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01C5, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01C6, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01C7, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01C8, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01C9, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01E4, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01E5, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01E6, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01E7, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01E8, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x01E9, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
	new VirtualDevice<uint8_t>(0x0245, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINMAX),
};
#endif




void PrintArrayHex(uint8_t *data, uint8_t length, bool prefix = true)
{
    for(uint8_t i = 0; i < length; ++i)
    {
        if(prefix == true) Serial.print("0x");
        if(data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    
    return;
}

void DumpDB()
{
    Serial.println("DumpDB: ");
    DB.Dump([](uint16_t id, StateDB::db_t &obj)
    {
        Serial.print(" > ID: "); Serial.print(id); Serial.println(":");
        Serial.print(" >> Length: "); Serial.print(obj.length); Serial.println(";");
        Serial.print(" >> Data: "); PrintArrayHex(obj.data, obj.length); Serial.println(";");
        Serial.print(" >> Time: "); Serial.print(obj.time); Serial.println(";");
        Serial.println();
    }, false);
    Serial.println();
    
    return;
}


// Колбеки обработчика BlockInfoDB //

void OnBlockCanSend(uint16_t id, uint8_t *data, uint8_t length)
{
	//DEBUG_LOG_TOPIC("OnBlock", "CanTx: id: 0x%04X, fId: 0x%02X, len: %d\n", id, data[0], length);
	
	L2ManagerObj.Send(id, data, length);
	return;
}

void OnBlockData(uint8_t *data, uint8_t length)
{
	//DEBUG_LOG_TOPIC("OnBlock", "Data: fId: 0x%02X, len: %d\n", data[0], length);
	
	L3ManagerObj.AddPacketCommit(L3DriverInterface::DEVTYPE_ALL, data, length);
	return;
}

void OnBlockError(uint16_t id, L3BlockInfoDB::error_t code)
{
	DEBUG_LOG_TOPIC("OnBlock", "Error: id: 0x%04X, code: 0x%02X\n", id, code);
	
	return;
}

//  //








#include "esp_pm.h"
#include "esp_clk.h"

void setup()
{



setCpuFrequencyMhz(240);
esp_pm_config_esp32s3_t pm_config = {
    .max_freq_mhz = 240,
    .min_freq_mhz = 240,
    .light_sleep_enable = false
};
esp_pm_configure(&pm_config);










	Serial.begin(500000);
	delay(2000);
    Serial.println("Start Main ECU");


	About::Setup();
	Leds::Setup();
	Config::Setup();
	CANCore::Setup();
	SPICore::Setup();
	Analog::Setup();
	ScriptLogic::Setup();

	//SPI::Setup();


	// ------------------------------------------------------------------------------------
	DEBUG_LOG_TOPIC("CORE", "EEPROM Dump(%d): ", EEPROM.length());
	#ifdef DEBUG
	uint8_t data;
	for(uint16_t i = 0; i < EEPROM.length(); ++i)
	{
		if(i % 16 == 0)
		{
			Serial.printf("\r\n %04X | ", i);
		}
		
		if(i % 16 == 8)
		{
			Serial.print(" ");
		}
		
		data = EEPROM.readByte(i);
		Serial.printf("%02X ", data);
	}
	#endif
	DEBUG_LOG_SIMPLE(";\n");
	// ------------------------------------------------------------------------------------

    L2ManagerObj.SetDriver(L2TWAIObj);

	L3ManagerObj.AddDriver(L3DashboardObj);
	//L3ManagerObj.AddDriver(L3DriverBLEObj);

#if defined(USE_EMULATOR)
	for(auto &obj : emulator_objects)
	{
		emulator.RegDevice(*obj);
	}
#endif


	esp_task_wdt_delete(NULL);
    //rtc_wdt_protect_off();
    //rtc_wdt_disable();
	disableCore0WDT();
	disableCore1WDT();
	disableLoopWDT();


    return;
}

uint32_t current_time = 0;
uint32_t tick = 0;

void loop()
{
    current_time = millis();

	About::Loop(current_time);
	Leds::Loop(current_time);
	Config::Loop(current_time);
	CANCore::Loop(current_time);
	SPICore::Loop(current_time);
	Analog::Loop(current_time);
	ScriptLogic::Loop(current_time);

	//SPI::Loop(current_time);

	L2ManagerObj.Processing();

	L3ManagerObj.Processing();

	DB.Processing([](uint16_t id, StateDB::db_t &obj)
	{
		// Выполняем скрипты для id
		ScriptLogic::ScriptObj.Trigger(id, obj.data, obj.length);
		
		// Получаем подписанные устройства для id и отправляем данные
		uint8_t subs = SubsDB.GetDevices(id);

		L3Fid::can_msg_t packet = {};
		packet.can_id = id;
		packet.SetData(obj.data, obj.length);
		L3ManagerObj.AddPacket(subs, (uint8_t *)&packet, packet.GetLen());
	});
	L3ManagerObj.Commit(L3DriverInterface::DEVTYPE_ALL);

	BlockInfoDB.Processing(current_time);







#if defined(USE_EMULATOR)
	emulator.Processing(current_time);
#endif
    

    return;
}









void L3OnOpen(L3Driver *driver)
{
	DEBUG_LOG_TOPIC("L3", "Session open, dev 0x%02X\n", driver->GetType());
	
	return;
}

void L3OnClose(L3Driver *driver, L3DriverInterface::close_reason_t reason)
{
	DEBUG_LOG_TOPIC("L3", "Session close, dev 0x%02X, reason %u\n", driver->GetType(), reason);
	
	SubsDB.Unsubscribe(driver->GetType());

	return;
}

void L3OnRx(L3Driver *driver, const uint8_t *msg, uint16_t length)
{
	L3DriverInterface::dev_type_t dev_type = driver->GetType();
	L3Fid::fid_t fId = (L3Fid::fid_t)msg[0];
	
	switch(fId)
	{
		case L3Fid::FID_SUBSCRIBE:
		{
			const uint8_t count = msg[1];
			const uint8_t *data = &msg[2];
			
			StateDB::db_t db_obj;
			for(uint8_t i = 0; i < (count * 2); i += 2)
			{
				const uint16_t id_raw = data[i] | (uint16_t(data[i+1]) << 8);
				const uint16_t id = id_raw & 0x7FFF;
				const bool unsubscribe = id_raw >> 15;
				
				// Подписка
				if(unsubscribe == false)
				{
					DEBUG_LOG_TOPIC("L3_SUB_PACK", "(%d/%d), ID: 0x%04X\n", (i / 2)+1, count, id);
					
					SubsDB.Subscribe(id, dev_type, false);
					
					if(DB.Get(id, db_obj) == true)
					{
						// Отправляем в устройство текущее значение.
						
						L3Fid::can_msg_t packet = {};
						packet.can_id = id;
						packet.SetData(db_obj.data, db_obj.length);
						driver->AddPacket((uint8_t *)&packet, packet.GetLen());
					}
					else
					{
						// Отправляем в устройство ошибку отсутствия данных.
						//uint8_t data[] = {0xE0};
						//L3.Send(dev, L3_REQTYPE_ERROR, id, data, sizeof(data));
						
						// Отправляем в CAN пакет запроса значения.
						L2Frame l2packet = {};
						l2packet.id = id;
						l2packet.length = 1;
						l2packet.fId = 0x11;
						L2ManagerObj.Send(l2packet);
					}
				}
				// Отписка
				else
				{
					//DEBUG_LOG_TOPIC("L3_UNSUB_PACK", "(%d/%d), ID: 0x%04X\n", (i / 2)+1, (id_array_len / 2), id);
					
					SubsDB.Unsubscribe(id, dev_type);
					
					// Отвечаем пустым значением.
					//response.Type(L3_REQTYPE_EVENTS);
					//response.Param(id);
				}
			}
			driver->Commit();

			break;
		}
		
		// Пакет can_msg_t от L3 устройства
		case L3Fid::FID_CAN_MSG:
		{
			const L3Fid::can_msg_t *packet = (const L3Fid::can_msg_t *)msg;
			
			DEBUG_LOG_TOPIC("L3_ReqEvent", "ID: %d, %RawPacket(%d): ", packet->can_id, packet->length);
			DEBUG_LOG_ARRAY_HEX(nullptr, packet->data, packet->length);
			DEBUG_LOG_SIMPLE(";\n");
			
			SubsDB.Subscribe(packet->can_id, dev_type, true);
			L2ManagerObj.Send(packet->can_id, packet->data, packet->length);
			
			/*
            else
			{
				// Отвечаем ошибкой.
				response.Type(L3_REQTYPE_ERROR);
				response.Param( request.Param() );
				response.PutData( 0xEE );
			}
			*/
			
			break;
		}

		case L3Fid::FID_SCRIPT_CTRL:
		{
			uint8_t tx[ScriptProto::MSG_MAX];
			uint16_t tx_len;
			
			ScriptProto::Rx(msg, length, tx, tx_len);
			if(tx_len > 0)
			{
				driver->AddPacket(tx, tx_len);
				//driver->Commit();	// TODO: если менеджер сам делает Commit() после разбора пакета — убрать, ответы будут упакованы вместе
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

void L3OnError(L3Driver *driver, L3C::error_t error)
{
	DEBUG_LOG_TOPIC("L3", "Error, dev 0x%02X: %s\n", driver->GetType(), L3C::ErrorName(error));
	
	return;
}












/*
// Подключение устройства по протоколу L3.
void L3OnConnect(L3DevType_t dev, int8_t code)
{
	if(code == 2)
	{
		//BlockInfoDB.GetAllIter([&](uint8_t *data, uint8_t length)
		//{
		//	L3.Send(dev, L3_REQTYPE_NONE05, 1, data, length);
		//});
	}

	return;
}
*/





// Приём пакета по протоколу L2.
void L2OnRx(const L2Frame &frame, uint32_t time)
{
	//DEBUG_LOG_TOPIC("L2_OnRX", "addr: 0x%04X, len: %d, data: ", request.id, request.length);
	//DEBUG_LOG_ARRAY_HEX(nullptr, request.data, request.length);
	//DEBUG_LOG_SIMPLE(";\n");

	//#warning Remove this after debug !!!1
	//DB.SetObjType(request.id, 0x01);
	
	// Получен ответ на запрос типа CAN объекта.
	//if(request.length == 2 && request.data[0] == 0x7A)
	//{
	//	DB.SetObjType(request.id, request.data[1]);
	//
	//	DEBUG_LOG_TOPIC("L2_DEB", "SET, addr: 0x%04X, type: 0x%02X;\n", request.id, request.data[1]);
	//}
	// Получен любой другой CAN пакет.
	//else
	{
		// Тип CAN объекта неизвестен.
		//if(DB.GetObjType(request.id) == 0)
		//{
			/*
			response.address = request.address;
			response.extended = false;
			response.rtr = false;
			response.dlc = 0;
			response.length = 1;
			response.data[0] = 0x3A;

			result = true;
			

			DEBUG_LOG_TOPIC("L2_DEB", "GET, addr: 0x%04X;\n", request.address);
			// Отправить 3 раза и подождать минуту.
			*/
		//}

		BlockInfoDB.ISR_CanRx(frame.id, frame.raw, frame.length);

		static constexpr bool filter_table[256] = 
		{
			// x0     x1     x2     x3     x4     x5     x6     x7     x8     x9     xA     xB     xC     xD     xE     xF
			false, false, false, false, false, false, false, false, false, false,  true, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false,  true, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false,  true,  true,  true, false,  true, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false,  true, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
		};

		if(filter_table[frame.fId])
		{
			DB.Set(frame.id, frame.raw, frame.length, time);
		}
	}
	
	return;
}

// Ошибка приёма пакета по протоколу L2.
//void L2OnError(int8_t code)
void L2OnError(uint32_t errors, const L2Statistics &stats)
{
	DEBUG_LOG_TOPIC("L2_OnEr", "code: %d;\n", errors);
	
	return;
}
