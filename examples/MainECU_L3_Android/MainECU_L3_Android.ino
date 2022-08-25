/*
	Программа эмитирует запросы параметров.
	По сути эмулятор программы на Android.
	DIP - COM4
*/

#include <SoftwareSerial.h>
#include "packet.h"
#include "L3Driver.h"
#include "L3Wrapper.h"

enum type_t { TYPE_NONE, TYPE_BOOL, TYPE_UINT8, TYPE_UINT16, TYPE_UINT32, TYPE_INT8, TYPE_INT16, TYPE_INT32 };
enum state_t { STATE_IDLE, STATE_WAIT, STATE_RECEIVED, STATE_TIMEOUT };
struct device_t
{
	uint32_t id;		// ID параметра \ Датчика.
	type_t type;		// Способ интерпретации полученных данных.
	state_t state;		// Текущее состояние данных датчика.
};

device_t devices[] = 
{
	{18260, TYPE_UINT8},	// Скорость, км\ч.
	{54229, TYPE_UINT32},	// Напряжение АКБ, мВ.
	{41177, TYPE_INT32},	// Ток АКБ, мА.
	{23673, TYPE_BOOL},		// Состояние ближнего света, 1\0.
	{31516, TYPE_NONE}		// 
};
uint8_t devices_idx = 0;


//L3DriverRAW driver_raw;
L3DriverSoftSerial driver_ss;
L3Wrapper L3(0, driver_ss);


void setup()
{
	Serial.begin(115200);
	Serial.println("Start");
	
	for(auto &device : devices)
	{
		device.state = STATE_IDLE;
	}
	
	L3.RegCallback(OnRX);
	L3.Init();
	
	return;
}

uint32_t current_time = 0;
uint32_t tick_time = 0;
uint32_t pools_time = 0;

void loop()
{
	current_time = millis();
	
	L3.IncomingByte();
	
	if(current_time - pools_time > 1000)
	{
		if( devices[devices_idx].state == STATE_IDLE )
		{
			byte data[0];
			L3.Send(1, devices[devices_idx].id, data, sizeof(data));
			
			devices[devices_idx].state = STATE_WAIT;
			
			Serial.println(" > Send request ..");
		}
		
		if( devices[devices_idx].state == STATE_RECEIVED )
		{
			devices[devices_idx].state = STATE_IDLE;
			
			if(++devices_idx == sizeof(devices) / sizeof(device_t))
			{
				devices_idx = 0;
				
				pools_time = current_time;
			}
		}
		
		// Нужен обработчик STATE_TIMEOUT.
	}
	
	return;
}

bool OnRX(L3Wrapper::packet_t &request, L3Wrapper::packet_t &response)
{
	byte data_arr[ request.GetDataLength() ];
	for(uint8_t i = 0; i < request.GetDataLength(); ++i)
	{
		byte data;
		request.GetData(data);
		data_arr[i] = data;
	}
	
	if( devices[devices_idx].id == request.Param() )
	{
		devices[devices_idx].state = STATE_RECEIVED;
		
		if(request.Type() != 31)
		{
			Serial.println(" > Expected data received: ");
		}
		else
		{
			Serial.println(" > Error data received: ");
		}
	}
	else
	{
		// Получены не запрошенные данные.
		Serial.println(" > ACHTUNG data received: ");
	}
	
	Serial.print("Type: ");
	Serial.println( request.Type() );
	
	Serial.print("Param: ");
	Serial.println( request.Param() );
	
	Serial.print("RawData(");
	Serial.print(request.GetDataLength());
	Serial.print("): ");
	for(uint8_t i = 0; i < sizeof(data_arr); ++i)
	{
		if(data_arr[i] < 0x10) Serial.print("0");
		Serial.print(data_arr[i], HEX);
		Serial.print(" ");
	}
	Serial.println();
	
	Serial.print("Parsed: ");
	for(auto &device : devices)
	{
		if( device.id == request.Param() )
		{
			switch(device.type)
			{
				case TYPE_BOOL: { bool tmp; memcpy(&tmp, &data_arr, sizeof(data_arr)); Serial.println(tmp); break; }
				case TYPE_UINT8: { uint8_t tmp; memcpy(&tmp, &data_arr, sizeof(data_arr)); Serial.println(tmp); break; }
				case TYPE_UINT16: { uint16_t tmp; memcpy(&tmp, &data_arr, sizeof(data_arr)); Serial.println(tmp); break; }
				case TYPE_UINT32: { uint32_t tmp; memcpy(&tmp, &data_arr, sizeof(data_arr)); Serial.println(tmp); break; }
				case TYPE_INT8: { int8_t tmp; memcpy(&tmp, &data_arr, sizeof(data_arr)); Serial.println(tmp); break; }
				case TYPE_INT16: { int16_t tmp; memcpy(&tmp, &data_arr, sizeof(data_arr)); Serial.println(tmp); break; }
				case TYPE_INT32: { int32_t tmp; memcpy(&tmp, &data_arr, sizeof(data_arr)); Serial.println(tmp); break; }
			}
			
			break;
		}
	}
	
	Serial.println();
	Serial.println();
	
	return false;
}
