#include <Timer.h>
#include "hm.h"

configuration hmAppC {}
implementation 
{
	components MainC;
	components LedsC;
	components new TimerMilliC() as Timer0;
	components new TimerMilliC() as Timer1;
	components new TimerMilliC() as Timer2;
	components SerialActiveMessageC as Serial;
	components ActiveMessageC as Radio;
	components hmC as App;
	components RandomC;

	MainC.Boot <- App;
	App.Timer0 -> Timer0;
	App.Timer1 -> Timer1;
	App.Timer2 -> Timer2;
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
	App.RadioReceiveSensor -> Radio.Receive[EXT_TYPE];
	App.RadioReceiveFault -> Radio.Receive[FAULT_TYPE];
	App.RadioReceiveRoute -> Radio.Receive[NETWORK_TYPE];
	App.RadioReceiveAlive -> Radio.Receive[ALIVE_TYPE];
	//App.RadioReceiveSnoop -> Radio.Snoop;
	App.RadioPacket -> Radio;
	App.RadioAMPacket -> Radio;
}