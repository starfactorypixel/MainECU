/*
	Обёртка пакета L2.
*/

#pragma once

#include <stdint.h>
#include <L2Driver.h>

class L2Wrapper
{
    public:
        
        struct packet_t
        {
            uint16_t address;
            uint8_t data[8];
            uint8_t length;
        };
        using callback_event_t = bool (*)(packet_t &request, packet_t &response);
        using callback_error_t = void (*)(int8_t code);
        
        void Init()
        {
            this->_driver.Init();
            
            return;
        }
        
        void RegCallback(callback_event_t event, callback_error_t error = nullptr)
        {
            this->_callback_event = event;
            this->_callback_error = error;
            
            return;
        }

        bool Send(packet_t packet)
        {
            return true;
        }
        
        // Псевдокод, для понимая логики.
        void Processing()
        {
            if(/*IsReceived* == */ true)
            {
                packet_t _request;
                packet_t _response;
                
                if( this->_callback_event(_request, _response) == true )
                {
                    this->_Send();
                }
            }
        }
        
    private:
        
        bool _Send()
        {
            return true;
        }
        
        L2Driver _driver;
        callback_event_t _callback_event;
        callback_error_t _callback_error;

};
