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
	#ifdef REAL_VOLTAGE
	components new VoltageC() as Voltage;
	#endif

	MainC.Boot <- App;
	App.Timer0 -> Timer0;
	App.Timer1 -> Timer1;
	App.Timer2 -> Timer2;
	App.Leds -> LedsC;
	App.Random -> RandomC;
	#ifdef REAL_VOLTAGE
	App.Read -> Voltage;
	#endif
	
	App.SerialControl -> Serial;
	App.UartSend -> Serial;
	App.UartReceive -> Serial.Receive[NETWORK_TYPE];
	App.UartPacket -> Serial;
	App.UartAMPacket -> Serial;
	
	App.RadioControl -> Radio;
	App.RadioSend -> Radio;
	App.RadioReceiveSensor -> Radio.Receive[SENSOR_TYPE];
	App.RadioReceiveSensor -> Radio.Receive[EXT_TYPE];
	App.RadioReceiveSensor -> Radio.Receive[EXT2_TYPE];
	App.RadioReceiveSensor -> Radio.Receive[EXT3_TYPE];
	App.RadioReceiveSensor -> Radio.Receive[EXT4_TYPE];
	App.RadioReceiveFault -> Radio.Receive[FAULT_TYPE];
	App.RadioReceiveRoute -> Radio.Receive[NETWORK_TYPE];
	#ifndef NO_HM
	#ifndef SNOOP_MODE
	App.RadioReceiveAlive -> Radio.Receive[ALIVE_TYPE];
	#endif
	#ifdef SNOOP_MODE
	App.RadioReceiveSnoop -> Radio.Snoop;
	#endif
	#endif
	App.RadioPacket -> Radio;
	App.RadioAMPacket -> Radio;
}