/*
    Основная программа Main ECU модуля.
*/

#include "soc/rtc_wdt.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"

#include <Arduino.h>

#include <LoggerLibrary.h>
#include <Config.h>
#include <Security.h>

#include <StateDB.h>
#include <L2Wrapper.h>
#include <L3Wrapper.h>
#include <L3SubscriptionsDB.h>
#include <VirtualValue.h>


StateDB DB;



L2Wrapper L2;

bool L2OnRX(L2Wrapper::packet_t &request, L2Wrapper::packet_t &response);
void L2OnError(int8_t code);



L3DriverBluetooth L3Driver_BT;  // Для соединения по BT.
L3DriverUART L3Driver_UART;     // Для соединения по UART (rs485) с бортовым компьютером.
//L3DriverSerial driver_ss;     // Для соединения по Serial.
//L3Wrapper L3(0, driver_ss);
L3Wrapper L3;

bool L3OnRX(L3DevType_t dev, L3Wrapper::packet_t &request, L3Wrapper::packet_t &response);
void L3OnError(L3DevType_t dev, L3Wrapper::packet_t &packet, int8_t code);
void L3OnReset(L3DevType_t dev);

L3SubscriptionsDB SubsDB;


VirtualValue VV;


#if defined(USE_EMULATOR)
#include <Emulator.h>
void EmulatorOnUpdate(uint32_t id, uint8_t *bytes, uint8_t length, uint32_t time)
{
    uint8_t new_bytes[8];
    new_bytes[0] = 0x61;
    for(uint8_t i = 0; i < length; ++i)
    {
        new_bytes[i+1] = bytes[i];
    }

	if(id == 0x0106)
	{
		new_bytes[3] = new_bytes[1];
		new_bytes[4] = new_bytes[2];
		length += 2;
	}

    DB.Set(id, new_bytes, length+1, time);
    
    return;
}
Emulator em(EmulatorOnUpdate);
//									id,			min,		max,		interval,	step,		value,		algorithm
VirtualDevice<uint16_t>	dev_voltage(0x0044,		620,		820,		1000,		7,			743,		VirtualDevice<uint16_t>::ALG_MINFADEMAX);
VirtualDevice<uint16_t>	dev_speed(0x0106,		0,			1000,		300,		1,			2,			VirtualDevice<uint16_t>::ALG_MINFADEMAX);
VirtualDevice<int16_t>	dev_current(0x0045,		-1500,		1500,		1000,		20,			-112,		VirtualDevice<int16_t>::ALG_RANDOM);
//VirtualDevice<uint8_t>	dev_light(0x00E5,		0,			255,		5000,		1,			0,			VirtualDevice<uint8_t>::ALG_MINMAX);	// Стоп сигнал
VirtualDevice<int16_t>	dev_power(0x0054,		-5000,		5000,		1000,		250,		-1124,		VirtualDevice<int16_t>::ALG_RANDOM);
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
            L3Driver_BT.Tick( millis() );
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




void setup()
{
    Serial.begin(500000);
    Serial.println("Start Main ECU");

	Config::Setup();
	Security::Setup();


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

    
    L3.AddDevice(L3Driver_BT);
    L3.AddDevice(L3Driver_UART);
    L3.RegCallback(L3OnRX, L3OnError, L3OnReset);
    L3.Init();



#if defined(USE_EMULATOR)
	em.RegDevice(dev_voltage);
	em.RegDevice(dev_speed);
	em.RegDevice(dev_current);
	//em.RegDevice(dev_light);
	em.RegDevice(dev_power);
#endif


	esp_task_wdt_delete(NULL);
    rtc_wdt_protect_off();
    rtc_wdt_disable();
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

	Config::Loop(current_time);
	Security::Loop(current_time);

    L2.Processing(current_time);

    L3.Processing(current_time);
    
	DB.Processing(current_time, [](uint16_t id, StateDB::db_t &obj)
	{
		// Не смотри сюда, это бред, ужас и вообще позор всего С++.
		// Потом перепишу.. Обещаю :'(
		L3DevType_t subs = SubsDB.GetDev(id);

		if(id == 0x00E0)
		{
			L3PacketTypes::dev_info_t dev_info = {};
			dev_info.baseID = id;
			dev_info.type = (obj.data[1] >> 3);
			dev_info.hw_ver = (obj.data[1] & 0x07);
			dev_info.sw_ver = (obj.data[2] >> 2);
			dev_info.proto_ver = (obj.data[2] & 0x03);
			memcpy((void*)&dev_info.uptime, &obj.data[3], 4);
			
			L3.Send(L3_DEVTYPE_ALL, L3_REQTYPE_SERVICES, 0x0000, (uint8_t*)&dev_info, sizeof(dev_info));
		}
		
		if(subs == L3_DEVTYPE_NONE) return;
		
		if( (subs & L3_DEVTYPE_BLUETOOTH) != L3_DEVTYPE_NONE )
        {
			DEBUG_LOG_TOPIC("L3_Send", "BT id: 0x%04X", id);
			L3.Send(L3_DEVTYPE_BLUETOOTH, L3_REQTYPE_EVENTS, id, obj.data, obj.length);
			DEBUG_LOG_SIMPLE(" done;\n");
        }
        if( (subs & L3_DEVTYPE_DASHBOARD) != L3_DEVTYPE_NONE )
        {
			DEBUG_LOG_TOPIC("L3_Send", "DB id: 0x%04X", id);
            L3.Send(L3_DEVTYPE_DASHBOARD, L3_REQTYPE_EVENTS, id, obj.data, obj.length);
            DEBUG_LOG_SIMPLE(" done;\n");
        }
        if( (subs & L3_DEVTYPE_COMPUTER) != L3_DEVTYPE_NONE )
        {
			DEBUG_LOG_TOPIC("L3_Send", "UART id: 0x%04X", id);
            L3.Send(L3_DEVTYPE_COMPUTER, L3_REQTYPE_EVENTS, id, obj.data, obj.length);
            DEBUG_LOG_SIMPLE(" done;\n");
        }
    });


#if defined(USE_EMULATOR)
    em.Processing(current_time);
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
                SubsDB.Set(request.Param(), dev);
                
                StateDB::db_t db_obj;
				DB.Get(request.Param(), db_obj);
                
                // Отвечаем текущим значением.
				response.Type( request.Type() );
				response.Param( request.Param() );
				response.PutData( db_obj.data, db_obj.length );
			}
			else if( (request.Param() % 0x8000) < 0x0800 )
			{
				SubsDB.Del( (request.Param() % 0x8000), dev);
				
				// Отвечаем пустым значением.
				response.Type( request.Type() );
				response.Param( request.Param() );
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

        // Событие с телефона.
        case L3_REQTYPE_EVENTS:
        {
            if(request.Param() < 0x0800)
            {
                SubsDB.Set(request.Param(), dev, true);

                L2Wrapper::packet_t can_packet = { request.Param(), false, false, 0, request.GetDataLength() };
                for(uint8_t i = 0; i < request.GetDataLength(); ++i)
                {
                    can_packet.data[i] = data_ptr[i];
                }
                
                L2.Send(can_packet);
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
	
	SubsDB.DelDev(dev);
	
	return;
}




// Приём пакета по протоколу L2.
bool L2OnRX(L2Wrapper::packet_t &request, L2Wrapper::packet_t &response)
{
    bool result = false;
	
	DEBUG_LOG_TOPIC("L2_OnRX", "addr: 0x%04X, len: %d, data: ", request.address, request.length);
	DEBUG_LOG_ARRAY_HEX(nullptr, request.data, request.length);
	DEBUG_LOG_SIMPLE(";\n");
	
	DB.Set(request.address, request.data, request.length, millis());
	
	return result;
}

// Ошибка приёма пакета по протоколу L2.
void L2OnError(int8_t code)
{
	DEBUG_LOG_TOPIC("L2_OnEr", "code: %d;\n", code);
	
	return;
}
