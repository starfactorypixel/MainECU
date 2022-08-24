/*
	Промер программы эмулятора устройств.
*/

#include "emulator.h"

Emulator em;

VirtualDevice<float> dev_voltage(12345, 0.0, 120.0, 2500, 74.32);
VirtualDevice<uint8_t> dev_speed(12045, 0, 101, 750, 2);
VirtualDevice<int32_t> dev_current(3472, -150, 150, 1000, -1124);

void setup()
{
	Serial.begin(115200);
	
	em.RegDevice(dev_voltage);
	em.RegDevice(dev_speed);
	em.RegDevice(dev_current);
	em.Processing();
	
	return;
}

void loop()
{
	return;
}
