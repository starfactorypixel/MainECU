#pragma once
#include <inttypes.h>
#include <DrakePinD.hpp>

namespace CANCore
{
	DrakePinD rs({0, GPIO_NUM_6}, DrakePin::OutputOpenDrain, DrakePin::High);


	inline void Setup()
	{
		rs.Init();
		
		rs.Off();
		
		return;
	}
	
	inline void Loop(uint32_t &time)
	{
		
		
		time = millis();
		return;
	}
}
