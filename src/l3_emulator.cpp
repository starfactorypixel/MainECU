/*
	Программа эмитирует ответы от MainECU.
	По сути эмулятор блока MainECU с виртуальными датчиками.
	SMD - COM5
*/


#include <stdint.h>
#include <L3Packet.h>
#include <L3Driver.h>

#define DEBUG true				// Громкий режим, для отладки через OTG - false.
#define Serial if(DEBUG)Serial

#include <L3Wrapper.h>
#include <Emulator.h>

#ifdef ARDUINO_ARCH_ESP32
 L3DriverBluetooth driver_ss;	// Для соединения по BT.
 //L3DriverSerial driver_ss;	// Для соединения по Serial.
#elif ARDUINO_ARCH_AVR
 L3DriverSoftSerial driver_ss;
#endif

L3Wrapper L3(0, driver_ss);



Emulator em;

//								uint32_t id, T min, T max, uint16_t interval, T step, T value, algorithm_t algorithm
VirtualDevice<uint32_t> dev_voltage(54229,		62000,		82000,		2500,		250,		74320,		VirtualDevice<uint32_t>::ALG_MINFADEMAX);
VirtualDevice<uint8_t>    dev_speed(18260,		0,			101,		750,		1,			2,			VirtualDevice<uint8_t>::ALG_MINFADEMAX);
VirtualDevice<int32_t>  dev_current(41177,		-150000,	150000,		1000,		250,		-1124,		VirtualDevice<int32_t>::ALG_RANDOM);
VirtualDevice<bool>       dev_light(23673,		0,			1,			5000,		1,			0,			VirtualDevice<bool>::ALG_MINMAX);

bool OnRX(L3Wrapper::packet_t &request, L3Wrapper::packet_t &response);
void OnError(L3Wrapper::packet_t &packet, int8_t code);

void setup()
{
	Serial.begin(115200);
	Serial.println("Start");
	
	L3.RegCallback(OnRX, OnError);
	L3.Init();
	
	em.RegDevice(dev_voltage);
	em.RegDevice(dev_speed);
	em.RegDevice(dev_current);
	em.RegDevice(dev_light);
	
	return;
}

uint32_t current_time = 0;

void loop()
{
	current_time = millis();
	
	L3.IncomingByte();
	em.Processing(current_time);
	
	return;
}

bool OnRX(L3Wrapper::packet_t &request, L3Wrapper::packet_t &response)
{
	/*
	byte data_arr[ request.GetDataLength() ];
	for(uint8_t i = 0; i < request.GetDataLength(); ++i)
	{
		byte data;
		request.GetData(data);
		data_arr[i] = data;
	}
	*/

	uint8_t *packet_arr = request.GetPacketPtr();
	uint8_t *data_arr = request.GetDataPtr();

	Serial.print("RawPacket(");
	Serial.print(request.GetPacketLength());
	Serial.print("): ");
	for(uint8_t i = 0; i < request.GetPacketLength(); ++i)
	{
		if(packet_arr[i] < 0x10) Serial.print("0");
		Serial.print(packet_arr[i], HEX);
		Serial.print(" ");
	}
	Serial.println();
	
	Serial.print("Type: ");
	Serial.println( request.Type() );
	
	Serial.print("Param: ");
	Serial.println( request.Param() );
	
	Serial.print("RawData(");
	Serial.print(request.GetDataLength());
	Serial.print("): ");
	for(uint8_t i = 0; i < request.GetDataLength(); ++i)
	{
		if(data_arr[i] < 0x10) Serial.print("0");
		Serial.print(data_arr[i], HEX);
		Serial.print(" ");
	}
	Serial.println();
	
	
	
	uint8_t bytes[8];
	uint8_t length;
	if( em.Request(request.Param(), bytes, length) == true )
	{
		Serial.println("Send response ...");
		
		response.Type( request.Type() );
		response.Param( request.Param() );
		
		Serial.print("RawData(");
		Serial.print(length);
		Serial.print("): ");
		for(int8_t i = 0; i < length; ++i)
		{
			response.PutData(bytes[i]);
			
			if(bytes[i] < 0x10) Serial.print("0");
			Serial.print(bytes[i], HEX);
			Serial.print(" ");
		}
		Serial.println();
	}
	else
	{
		Serial.print("Send error ...");
		
		response.Type( 31 );
		response.Param( request.Param() );
		
		byte err_data[1];
		err_data[0] = 0xE1;
		response.PutData(err_data[0]);
	}
	
	Serial.println();
	Serial.println();
	
	return true;
}

void OnError(L3Wrapper::packet_t &packet, int8_t code)
{
	switch (code)
	{
		case L3Packet<>::ERROR_FORMAT:
		{
			Serial.println("ERROR_FORMAT");
			
			break;
		}
		case L3Packet<>::ERROR_VERSION:
		{
			Serial.println("ERROR_VERSION");
			
			break;
		}
		case L3Packet<>::ERROR_CRC:
		{
			Serial.println("ERROR_CRC");
			
			break;
		}
		case L3Packet<>::ERROR_OVERFLOW:
		{
			Serial.println("ERROR_OVERFLOW");
			
			break;
		}
		default:
		{
			break;
		}
	}
	
	Serial.print("Packet: '");
	byte rxbyte;
	while( packet.GetPacketByte(rxbyte) == true )
	{
		if(rxbyte < 0x10) Serial.print("0");
		Serial.print(rxbyte, HEX);
	}
	Serial.println("'");
	
	return;
}
