#include <Timer.h>
#include "hm.h"

configuration hmAppC {}
implementation 
{
	components MainC;
	components LedsC;
	components new TimerMilliC() as Timer0;
	components SerialActiveMessageC as Serial;
	components ActiveMessageC as Radio;
	components hmC as App;
	components RandomC;

	MainC.Boot <- App;
	App.Timer0 -> Timer0;
	App.Leds -> LedsC;
	App.Random -> RandomC;
	
	App.SerialControl -> Serial;
	App.UartSend -> Serial;
	App.UartReceive -> Serial.Receive[NETWORK_TYPE];
	App.UartPacket -> Serial;
	App.UartAMPacket -> Serial;
	
	App.RadioControl -> Radio;
	App.RadioSend -> Radio;
	App.RadioReceiveSensor -> Radio.Receive[SENSOR_TYPE];
	App.RadioReceiveFault -> Radio.Receive[FAULT_TYPE];
	App.RadioReceiveRoute -> Radio.Receive[NETWORK_TYPE];
	App.RadioPacket -> Radio;
	App.RadioAMPacket -> Radio;
}