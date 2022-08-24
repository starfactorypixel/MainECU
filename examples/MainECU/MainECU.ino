/*
	Старый код, на всякий случай.
	!!! НЕ ИСПОЛЬЗОВАТЬ !!!
*/

#include "packet.h"
//#include "emulator.h"
#include <SoftwareSerial.h>



class L3Driver
{
	public:
		virtual void Init(){ }
		virtual uint8_t ReadAvailable(){ }
		virtual byte ReadByte(){ }
		virtual void SendByte(byte data){ }
};

/*
	Драйвер работы с нативным uart.
	Использует пины: Согласно Serial.
	Скорость: 115200.
*/
class L3DriverRAW : public L3Driver
{
	public:
		void Init()
		{
			Serial.begin(115200);
			
			return;
		}
		
		uint8_t ReadAvailable() override
		{
			return Serial.available();
		}
		
		byte ReadByte() override
		{
			return Serial.read();
		}
		
		void SendByte(byte data) override
		{
			return Serial.write(data);
		}
};

class L3DriverUART : public L3Driver
{
	
};

class L3DriverBluetooth : public L3Driver
{
	
};


/*
	Драйвер работы с виртуальным uart.
	Использует пины: RX:2, RX:3.
	Скорость: 19200.
*/
class L3DriverSoftSerial : public L3Driver
{
	public:
		L3DriverSoftSerial() : mySerial(2, 3)
		{
			return;
		}
		
		void Init()
		{
			mySerial.begin(19200);
			
			return;
		}
		
		uint8_t ReadAvailable() override
		{
			return mySerial.available();
		}
		
		byte ReadByte() override
		{
			return mySerial.read();
		}
		
		void SendByte(byte data) override
		{
			return mySerial.write(data);
		}
	
	private:
		SoftwareSerial mySerial;
};





class L3Wrapper
{
	public:
		using packet_t = StarPixelHighPacket<64>;
		using callback_t = bool (*)(packet_t &request, packet_t &response);
		
		L3Wrapper(uint8_t transport, L3Driver &driver) : _driver(&driver)
		{
			this->_transport = transport;
			
			return;
		}
		
		void Init()
		{
			this->_driver->Init();
		}
		
		void RegCallback(callback_t callback)
		{
			this->_callback = callback;
		}
		
		void SetUrgent()
		{
			this->_urgent_data = true;
			
			return;
		}
		
		// В будущем будет заменён на прерывание приёма байта.
		void IncomingByte()
		{
			if( this->_driver->ReadAvailable() > 0 )
			{
				byte incomingByte = this->_driver->ReadByte();
				
				if( _rx_packet.PutPacketByte(incomingByte) == true )
				{
					if( _rx_packet.IsReceived() == true )
					{
						if( this->_callback(_rx_packet, _tx_packet) == true )
						{
							// Установка транспорта ( перенести в packet.h ? )
							_tx_packet.Transport(this->_transport);
							
							// Флаг ответа.
							_tx_packet.Direction(1);
							
							// Флаг необходимости передать срочное сообщение.
							if(this->_urgent_data == true) _tx_packet.Urgent(1);
							this->_urgent_data = false;
							
							// Отправка ответа.
							byte data;
							while( _tx_packet.GetPacketByte(data) == true )
							{
								this->_driver->SendByte(data);
							}
							
							// Очистка пакета.
							_tx_packet.Init();
						}
						_rx_packet.Init();
					}
					
					if(_rx_packet.GetError() < 0)
					{
						Serial.print("Error: ");
						Serial.println( _rx_packet.GetError() );
						
						_rx_packet.Init();
					}
				}
				else
				{
					Serial.println("ERROR_OVERFLOW");
					
					_rx_packet.Init();
					
					// Или метод FlushBuffer() ?
					while(this->_driver->ReadAvailable() > 0){ this->_driver->ReadByte(); }
				}
			}
			
			return;
		}
	
	private:
		packet_t _rx_packet;
		packet_t _tx_packet;
		callback_t _callback;
		
		L3Driver *_driver;
		
		uint8_t _transport;
		bool _urgent_data = false;
};




//L3DriverRAW driver_raw;
L3DriverSoftSerial driver_ss;
L3Wrapper Protocol(0, driver_ss);

















/*

Emulator em;


VirtualDevice<float> dev_voltage(12345, 0.0, 120.0, 2500, 74.32);
VirtualDevice<uint8_t> dev_speed(12045, 0, 101, 750, 2);
VirtualDevice<int32_t> dev_current(3472, -150, 150, 1000, -1124);

*/






















void setup()
{
	Serial.begin(115200);
	
	
/*
	em.RegDevice(dev_voltage);
	em.RegDevice(dev_speed);
	em.RegDevice(dev_current);
	em.Processing();
*/	
	
	
	Protocol.RegCallback(OnRX);
	Protocol.Init();
	
	return;
}

void loop()
{
	//if( Serial.available() > 0 )
	{
		Protocol.IncomingByte();
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
	
	
	Serial.print("Type: ");
	Serial.println( request.Type() );
	
	Serial.print("Param: ");
	Serial.println( request.Param() );
	
	if(request.Param() == 65000)
	{
		uint32_t time = (((uint32_t)data_arr[0] << 24) | ((uint32_t)data_arr[1] << 16) | ((uint32_t)data_arr[2] << 8) | ((uint32_t)data_arr[3]));
		
		Serial.print("Time: ");
		Serial.println( time );
	}
	
	Serial.print("GetData: ");
	for(uint8_t i = 0; i < sizeof(data_arr); ++i)
	{
		if(data_arr[i] < 0x10) Serial.print("0");
		Serial.print(data_arr[i], HEX);
		Serial.print(" ");
	}
	
	Serial.println();
	Serial.println();
}

















/* ------------- RX -------------- */
/*
StarPixelHighPacket<64> RXObj;

void setup()
{
	
	
	
	
	
	
	
	
	
	
	
	
	
	Serial.begin(115200);
	
	RXObj.Transport(3);
	RXObj.Direction(1);
	
	return;
}

void loop()
{
	
	while(Serial.available() > 0)
	{
		byte incomingByte = Serial.read();
		
		if( RXObj.PutPacketByte(incomingByte) == true )
		{
			int8_t error;
			if( RXObj.IsReceived(error) == true )
			{
				Serial.print("Type: ");
				Serial.println( RXObj.Type() );
				
				Serial.print("Param: ");
				Serial.println( RXObj.Param() );
				
				if(RXObj.Param() == 65000)
				{
					
					uint32_t time = 0;
					
					for(int8_t i = 3; i >= 0; --i)
					{
						byte data;
						RXObj.GetData(data);
						time |= (data << i*8);
					}
					
					Serial.print("Time: ");
					Serial.println( time );
				
				}
				else
				{
					Serial.print("GetData: ");
					byte data;
					while( RXObj.GetData(data) == true )
					{
						Serial.print(data, HEX);
						Serial.print(" ");
					}
					Serial.println();
				}
				
				RXObj.Init();
				
				Serial.println();
			}
			
			if(error < 0)
			{
				Serial.print("Error: ");
				Serial.println( error );
				
				RXObj.Init();
			}
		}
		else
		{
			Serial.println("ERROR_OVERFLOW");
			
			RXObj.Init();
			while(Serial.available() > 0){ Serial.read(); }
		}
	}
	
	return;
}
*/
/* ------------- RX -------------- */









/* ------------- TX -------------- */
/*
SoftwareSerial txserial(2, 3);

StarPixelHighPacket<64> TXObj;

void setup()
{
	txserial.begin(19200);
	
	TXObj.Transport(3);
	TXObj.Type(5);
	TXObj.Param(21464);
	
	TXObj.Data1(0x12);
	TXObj.Data1(0x37);
	TXObj.Data1(0x4A);
	TXObj.Data1(0x39);
	TXObj.Data1(0xEA);
	TXObj.Data1(0x69);
	
	TXObj.Prepare();
	
	byte data;
	while( TXObj.GetPacketByte(data) == true )
	{
		txserial.write(data);
	}
	
	return;
}

void loop()
{
	TXObj.Init();
	
	TXObj.Transport(3);
	TXObj.Type(30);
	TXObj.Param(65000);
	
	uint32_t time = millis();
	for(int8_t i = 3; i >= 0; --i)
	{
		TXObj.Data1( ((time >> i*8) & 0xFF) );
	}
	
	TXObj.Prepare();
	
	byte data;
	while( TXObj.GetPacketByte(data) == true )
	{
		txserial.write(data);
	}
	
	delay(750);
	
	return;
}
*/
/* ------------- TX -------------- */