//#include "soc/rtc_wdt.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"

#include <Arduino.h>

#include <LoggerLibrary.h>
#include "About.h"
#include "Leds.h"
#include "Config.h"
#include "Security.h"

#include <StateDB.h>
#include <L2Wrapper.h>
#include "CANCore.h"
#include <L3Wrapper.h>
#include <L3SubscriptionDB.h>
#include <VirtualValue.h>

#include "SPICore.h"
#include "Analog.h"
#include "ScriptLogic.h"



uint32_t global_error_count = 0;



L2Wrapper L2;

bool L2OnRX(L2Wrapper::packet_t &request, L2Wrapper::packet_t &response);
void L2OnError(int8_t code);



//L3DriverBluetooth L3Driver_BT;  // Для соединения по BT.
L3DriverUART L3Driver_UART;     // Для соединения по UART (rs485) с бортовым компьютером.
//L3DriverSerial driver_ss;     // Для соединения по Serial.
//L3Wrapper L3(0, driver_ss);
L3Wrapper L3;

bool L3OnRX(L3DevType_t dev, L3Wrapper::packet_t &request, L3Wrapper::packet_t &response);
void L3OnError(L3DevType_t dev, L3Wrapper::packet_t &packet, int8_t code);
void L3OnReset(L3DevType_t dev);

StateDB DB;
L3SubscriptionDB SubsDB;


VirtualValue VV;


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
	new VirtualDevice<uint8_t>(0x01C4, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01C5, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01C6, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01C7, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01C8, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01C9, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01E4, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01E5, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01E6, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01E7, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01E8, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x01E9, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
	new VirtualDevice<uint8_t>(0x0245, 0x65, 0, 255, 5000, 1, 1, VirtualDeviceInterface::ALG_MINFADEMAX),
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


volatile uint8_t timet_iter = 0;
void IRAM_ATTR onTimer()
{
	cli();
	
	switch(timet_iter)
    {
        case 0:
        {
            //L3Driver_BT.Tick( millis() );
            timet_iter = 1;
            break;
        }
        case 1:
        {
            L3Driver_UART.Tick( millis() );
            timet_iter = 2;
            break;
        }
        default:
        {
            timet_iter = 0;
            break;
        }
	}
	
	sei();
}


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
	Security::Setup();
	CANCore::Setup();
	SPICore::Setup();
	Analog::Setup();
	ScriptLogic::Setup();

	//SPI::Setup();


	// ------------------------------------------------------------------------------------
	Logger.PrintTopic("CORE").Print("Serial Number: ").Print(Config::obj.security.serial, sizeof(Config::obj.security.serial), LOG_OUT_TYPE_HEX).PrintNewLine();
	
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

    
    L2.RegCallback(L2OnRX, L2OnError);
    L2.Init();

	//SPI::Dev_Active(SPI::PIN_CS_CAN_RS);

    
    //L3.AddDevice(L3Driver_BT);
    L3.AddDevice(L3Driver_UART);
    L3.RegCallback(L3OnRX, L3OnError, L3OnReset);
    L3.Init();



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
	
	
	hw_timer_t *My_timer = NULL;
    My_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(My_timer, &onTimer, true);
    timerAlarmWrite(My_timer, 5000, true);
    timerAlarmEnable(My_timer);
	
	
	VV.RegHandler(1000, [](VirtualValue::db_t obj)
    {
        static int32_t old_value = 0;
        static uint32_t old_time = 0;

        uint8_t delta_speed = abs( (obj.new_value - old_value) );
        float tmp = (delta_speed / 3.6) * (obj.new_time - old_time);
        obj.value = llrintf(tmp);



    });
    
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
	Security::Loop(current_time);
	CANCore::Loop(current_time);
	SPICore::Loop(current_time);
	Analog::Loop(current_time);
	ScriptLogic::Loop(current_time);

	//SPI::Loop(current_time);

    L2.Processing(current_time);

    L3.Processing(current_time);

	static uint32_t wqeqqwe = 0;
	if(millis() - wqeqqwe > 1000)
	{
		wqeqqwe = millis();
		DEBUG_LOG_TOPIC("SUB_LIST", "");
		SubsDB.Dump(L3_DEVTYPE_COMPUTER, [](uint16_t id)
		{
			DEBUG_LOG_SIMPLE("0x%04x, ", id);
		});
		DEBUG_LOG_SIMPLE("err: %d\n", global_error_count);
	}
    
	DB.Processing([](uint16_t id, StateDB::db_t &obj)
	{
		// Выполняем скрипты для id
		ScriptLogic::ScriptObj.Trigger(id, obj.data, obj.length);
		
		// Получаем подписанные устройства для id и отправляем данные
		uint8_t subs = SubsDB.GetDevices(id);
		while(subs)
		{
			uint8_t bit = subs & -subs;
			//DEBUG_LOG_TOPIC("L3_Send", "Dev: 0x%02X, ID: 0x%04X ...", bit, id);
			L3.Send((L3DevType_t)bit, L3_REQTYPE_EVENTS, id, obj.data, obj.length);
			//DEBUG_LOG_SIMPLE(" done;\n");
			subs &= ~bit;
		}
	});



/*
	static uint32_t tmp_tick = 0;
	uint32_t qqq_time = millis();
	if(qqq_time - tmp_tick >= 100)
	{
		DEBUG_LOG_TOPIC("TICK", "delta: %d\n", (qqq_time - tmp_tick));

		tmp_tick = qqq_time;
	}
*/







#if defined(USE_EMULATOR)
	emulator.Processing(current_time);
#endif
    

    return;
}




// Приём пакета по протоколу L3. Реализовано.
bool L3OnRX(L3DevType_t dev, L3Wrapper::packet_t &request, L3Wrapper::packet_t &response)
{
	bool result = false;
	
	uint8_t *packet_ptr = request.GetPacketPtr();
	uint8_t *data_ptr = request.GetDataPtr();
	
	DEBUG_LOG_TOPIC("L3_OnRX", "Type: 0x%02X, Param: 0x%04X, Data(%d): ", request.Type(), request.Param(), request.GetDataLength());
	DEBUG_LOG_ARRAY_HEX(nullptr, data_ptr, request.GetDataLength());
	DEBUG_LOG_SIMPLE(";\n");
	
	// https://wiki.starpixel.org/books/mainecu/page/protokol-l3#bkmrk-%D0%A2%D0%B8%D0%BF%D1%8B-%D0%B7%D0%B0%D0%BF%D1%80%D0%BE%D1%81%D0%B0
    switch (request.Type())
    {
        case 0x00:
        {
            // Все сервисные флаги будут установлены автоматически.
            response = request;
            result = true;
            
            break;
        }
        /*
        case 0x01:
        {
            StateDB::db_t db_obj;
            if( DB.Get(request.Param(), db_obj) == true )
            {
                response.Type( request.Type() );
                response.Param( request.Param() );
                response.PutData( db_obj.data, db_obj.length );
            }
            else
            {
                response.Type(L3_REQTYPE_ERROR);
                response.Param( request.Param() );
                response.PutData( 0x01 );
            }
            result = true;
            
            break;
        }
        */

		// Запрос на регистрацию подсписки на параметр. Param() < 32768 = подписка, Param() > 32767 = отписка.
		// Если ID подходящий (<2048), то отмечаем флаг в БД подписок и отвечаем с текущим параметром из БД состояний.
		// Если ID не подходящий (>2048), то отправляем ошибку.
		case L3_REQTYPE_SUBSCRIBE:
		{
			if(request.Param() < 0x0800)
			{
				SubsDB.Subscribe(request.Param(), dev, false);

				DEBUG_LOG_TOPIC("L3_SUB", "ID: 0x%04X\n", request.Param());
				
				StateDB::db_t db_obj;
				#warning return false if not set data. Send to L3 empty data!
				
				if( DB.Get(request.Param(), db_obj) == true )
				{
					// Отправляем в устройство текущее значение.
					response.Type( request.Type() );
					response.Param( request.Param() );
					response.PutData( db_obj.data, db_obj.length );
				}
				else
				{
					// Отправляем в устройство ошибку отсутствия данных.
					response.Type(L3_REQTYPE_ERROR);
					response.Param( request.Param() );
					response.PutData( 0xE0 );
					
					// Отправляем в CAN пакет запроса значения.
					L2Wrapper::packet_v2_t can_packet;
					can_packet.id = request.Param();
					can_packet.raw_data_len = 1;
					can_packet.func_id = 0x11;
					L2.Send(can_packet);
				}
                
                // Отвечаем текущим значением.
				//response.Type( request.Type() );
				//response.Param( request.Param() );
				//response.PutData( db_obj.data, db_obj.length );
			}
			else if( (request.Param() % 0x8000) < 0x0800 )
			{
				SubsDB.Unsubscribe( (request.Param() % 0x8000), dev );

				DEBUG_LOG_TOPIC("L3_UNSUB", "ID: 0x%04X\n", request.Param());
				
				// Отвечаем пустым значением.
				response.Type( request.Type() );
				response.Param( request.Param() );	// надо (request.Param() % 0x8000)
			}
			else
			{
				// Отвечаем ошибкой.
				response.Type(L3_REQTYPE_ERROR);
				response.Param( request.Param() );
				response.PutData( 0xEE );
			}
			
			result = true;
			
			break;
		}

		case L3_REQTYPE_SUBSCRIBE_PACK:
		{
			uint8_t *id_array_ptr = request.GetDataPtr();
			uint8_t id_array_len = request.GetDataLength();
			
			//DEBUG_LOG_ARRAY_HEX("SUB_PACK_hex", request.GetPacketPtr(), request.GetPacketLength());
			//DEBUG_LOG_NEW_LINE();
			
			if(id_array_len % 2 != 0) break;
			
			uint16_t id_raw;
			uint16_t id;
			uint8_t unsubscribe;
			StateDB::db_t db_obj;
			for(uint8_t i = 0; i < id_array_len; i += 2)
			{
				id_raw = *((uint16_t*)&id_array_ptr[i]);
				id = id_raw & 0x7FFF;
				unsubscribe = id_raw >> 15;
				
				// Подписка
				if(unsubscribe == 0)
				{
					//DEBUG_LOG_TOPIC("L3_SUB_PACK", "(%d/%d), ID: 0x%04X\n", (i / 2)+1, (id_array_len / 2), id);
					
					SubsDB.Subscribe(id, dev, false);
					
					if( DB.Get(id, db_obj) == true )
					{
						// Отправляем в устройство текущее значение.
						L3.Send(dev, L3_REQTYPE_EVENTS, id, db_obj.data, db_obj.length);
					}
					else
					{
						// Отправляем в устройство ошибку отсутствия данных.
						uint8_t data[] = {0xE0};
						L3.Send(dev, L3_REQTYPE_ERROR, id, data, sizeof(data));
						
						// Отправляем в CAN пакет запроса значения.
						L2Wrapper::packet_v2_t can_packet;
						can_packet.id = id;
						can_packet.raw_data_len = 1;
						can_packet.func_id = 0x11;
						L2.Send(can_packet);
					}
				}
				// Отписка
				else
				{
					//DEBUG_LOG_TOPIC("L3_UNSUB_PACK", "(%d/%d), ID: 0x%04X\n", (i / 2)+1, (id_array_len / 2), id);
					
					SubsDB.Unsubscribe(id, dev);
					
					// Отвечаем пустым значением.
					//response.Type(L3_REQTYPE_EVENTS);
					//response.Param(id);
				}
			}
			result = false;
			
			break;
		}

        // Событие с телефона.
        case L3_REQTYPE_EVENTS:
        {
			//DEBUG_LOG_TOPIC("L3", "L3_REQTYPE_EVENTS");
			//DEBUG_LOG_NEW_LINE();

			DEBUG_LOG_TOPIC("L3_ReqEvent", "ID: %d, %RawPacket(%d): ", request.Param(), request.GetDataLength());
			DEBUG_LOG_ARRAY_HEX(nullptr, data_ptr, request.GetDataLength());
			DEBUG_LOG_SIMPLE(";\n");

            if(request.Param() < 0x0800)
            {
                SubsDB.Subscribe(request.Param(), dev, true);

/*
                //L2Wrapper::packet_t can_packet = { request.Param(), false, false, 0, request.GetDataLength() };
				L2Wrapper::packet_t can_packet = {};
				can_packet.id = request.Param();
				can_packet.length = request.GetDataLength();
                for(uint8_t i = 0; i < request.GetDataLength(); ++i)
                {
                    can_packet.data[i] = data_ptr[i];
                }
                
                L2.Send(can_packet);
*/
				L2.Send( request.Param(), data_ptr, request.GetDataLength() );
            }
            else
			{
				// Отвечаем ошибкой.
				response.Type(L3_REQTYPE_ERROR);
				response.Param( request.Param() );
				response.PutData( 0xEE );
			}

            break;
        }
        
        /*
        case 0x19:
        {
            if( DB.Set( request.Param(), data_ptr, request.GetDataLength(), request.GetPacketTime() ) == true )
            {
                StateDB::db_t db_obj;
                DB.Get(request.Param(), db_obj);
                
                response.Type( request.Type() );
                response.Param( request.Param() );
                response.PutData( db_obj.data, db_obj.length );
            }
            else
            {
                response.Type( 0x1E );
                response.Param( request.Param() );
                response.PutData( 0x03 );
            }
            result = true;
            
            break;
        }
        */
        case 0x1A:
        {
            DumpDB();

            break;
        }
		case 0x1B:
		{
			result = ScriptLogic::L3Rx(request, response);
			break;
		}
        default:
        {
            response.Type(L3_REQTYPE_ERROR);
            response.Param( request.Param() );
            response.PutData( 0x02 );
            result = true;
            
            break;
        }
    }
    
    return result;
}

// Ошибка приёма пакета по протоколу L3. Реализовано.
void L3OnError(L3DevType_t dev, L3Wrapper::packet_t &packet, int8_t code)
{
	uint8_t *packet_ptr = packet.GetPacketPtr();
	
	DEBUG_LOG_TOPIC("L3_OnEr", "RawPacket(%d): ", packet.GetPacketLength());
	DEBUG_LOG_ARRAY_HEX(nullptr, packet_ptr, packet.GetPacketLength());
	DEBUG_LOG_SIMPLE(";\n");
	
	switch(code)
	{
		case packet.ERROR_FORMAT:
		{
			DEBUG_LOG_TOPIC("L3_OnEr", "code: %c;\n", "ERROR_FORMAT");
			
			break;
		}
		case packet.ERROR_VERSION:
		{
			DEBUG_LOG_TOPIC("L3_OnEr", "code: %c;\n", "ERROR_VERSION");
			
			break;
		}
		case packet.ERROR_CRC:
		{
			DEBUG_LOG_TOPIC("L3_OnEr", "code: %c;\n", "ERROR_CRC");
			
			break;
		}
		case packet.ERROR_OVERFLOW:
		{
			DEBUG_LOG_TOPIC("L3_OnEr", "code: %c;\n", "ERROR_OVERFLOW");
			
			break;
		}
		case packet.ERROR_TIMEOUT:
		{
			DEBUG_LOG_TOPIC("L3_OnEr", "code: %c;\n", "ERROR_TIMEOUT");
			
			break;
		}
		default:
		{
			DEBUG_LOG_TOPIC("L3_OnEr", "code: %d;\n", code);
			
			break;
		}
	}
	
	return;
}

void L3OnReset(L3DevType_t dev)
{
	DEBUG_LOG_TOPIC("L3_OnRst", "dev: 0x%02X;\n", dev);
	
	SubsDB.Unsubscribe(dev);
	
	return;
}




// Приём пакета по протоколу L2.
bool L2OnRX(L2Wrapper::packet_t &request, L2Wrapper::packet_t &response)
{
	bool result = false;
	
	DEBUG_LOG_TOPIC("L2_OnRX", "addr: 0x%04X, len: %d, data: ", request.id, request.length);
	DEBUG_LOG_ARRAY_HEX(nullptr, request.data, request.length);
	DEBUG_LOG_SIMPLE(";\n");

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



		static constexpr bool filter_table[256] = 
		{
			// x0     x1     x2     x3     x4     x5     x6     x7     x8     x9     xA     xB     xC     xD     xE     xF
			false, false, false, false, false, false, false, false, false, false,  true, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
			false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, 
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

		if(filter_table[request.data[0]])
		{
			DB.Set(request.id, request.data, request.length, millis());
		}
	}
	
	return result;
}

// Ошибка приёма пакета по протоколу L2.
void L2OnError(int8_t code)
{
	DEBUG_LOG_TOPIC("L2_OnEr", "code: %d;\n", code);
	
	return;
}
