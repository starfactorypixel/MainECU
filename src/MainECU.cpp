/*
    Основная программа Main ECU модуля.
*/

#include <Arduino.h>
#include <StateDB.h>

StateDB<> DB;

void setup()
{
    Serial.begin(115200);
    Serial.println("Start Main ECU");
    
    return;
}

void loop()
{
    static uint32_t current_time = millis();
    
    return;
}
