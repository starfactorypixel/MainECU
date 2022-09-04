/*
    Основная программа Main ECU модуля.
*/

#include <Arduino.h>

#include <StateDB.h>
#include <L3Wrapper.h>

StateDB<> DB;

L3DriverBluetooth driver_ss;    // Для соединения по BT.
//L3DriverSerial driver_ss;     // Для соединения по Serial.
L3Wrapper L3(0, driver_ss);

bool L3OnRX(L3Wrapper::packet_t &request, L3Wrapper::packet_t &response);
void L3OnError(L3Wrapper::packet_t &packet, int8_t code);


void setup()
{
    Serial.begin(115200);
    Serial.println("Start Main ECU");
    
    L3.RegCallback(L3OnRX, L3OnError);
    L3.Init();
    
    return;
}

void loop()
{
    static uint32_t current_time = millis();

    
    return;
}


// Приём пакета по протоколу L3. Реализовано.
bool L3OnRX(L3Wrapper::packet_t &request, L3Wrapper::packet_t &response)
{
    bool result = false;
    
    uint8_t *data_ptr = request.GetDataPtr();
    
    
    return result;
}

// Ошибка приёма пакета по протоколу L3. Реализовано.
void L3OnError(L3Wrapper::packet_t &packet, int8_t code)
{
    uint8_t *packet_ptr = packet.GetPacketPtr();
    
    
    return;
}

// Приём пакета по протоколу L2. Не реализовано.
bool L2OnRX(/* Входящий объект с данными, Исходящий объект с данными */)
{
    bool result = false;
    
    
    return result;
}

// Ошибка приёма пакета по протоколу L2. Не реализовано.
void L2OnError(/* Объект с сырыми входящими данными, Код ошибки */)
{
    return;
}
