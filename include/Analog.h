#pragma once
#include <inttypes.h>
#include <DrakePinA.hpp>
#include <CUtils.h>

namespace Analog
{
	DrakePinA adc_in({ADC1_CHANNEL_0}, 0);
	DividerVoltageCalc v2(12, 3260, 69000, 10000);
	
	
	inline void Setup()
	{
		adc_in.Init();
		
		return;
	}
	
	inline void Loop(uint32_t &time)
	{
		static uint32_t tick5000 = 0;
		if(time - tick5000 > 5000)
		{
			tick5000 = time;

			uint32_t val = adc_in.ReadRaw();
			DEBUG_LOG_TOPIC("ADC", "Vin: %dmv\n", v2.GetmV(val));
		}
		
		time = millis();
		return;
	}
};
