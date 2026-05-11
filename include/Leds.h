#pragma once
#include <inttypes.h>
#include <DrakePinD.hpp>

namespace Leds
{


	DrakePinD leds[] = 
	{
		{{0, GPIO_NUM_35}, DrakePin::Output, DrakePin::Low},	// W
		{{0, GPIO_NUM_36}, DrakePin::Output, DrakePin::Low},	// G
		{{0, GPIO_NUM_39}, DrakePin::Output, DrakePin::Low}		// R
	};
	
	enum leds_t : uint8_t
	{
		LED_WHITE = 0,
		LED_GREEN = 1,
		LED_RED = 2
	};




	inline void Setup()
	{
		for(auto &led : leds)
		{
			led.Init();
		}

		leds[LED_WHITE].On();
		delay(100);
		leds[LED_WHITE].Off();
		
		leds[LED_GREEN].On();
		delay(100);
		leds[LED_GREEN].Off();
		
		leds[LED_RED].On();
		delay(100);
		leds[LED_RED].Off();
		
		return;
	}

	inline void Loop(uint32_t &time)
	{
		
		time = millis();
		return;
	}
};
