#include "Timer.h"
#include "AM.h"
#include "Serial.h"
#include "hm.h"

module hmC @safe()
{
	uses
	{
		interface Boot;
		interface Leds;
		interface Timer<TMilli> as Timer0;
		interface Random;
		
		interface SplitControl as SerialControl;
		interface SplitControl as RadioControl;
		
		interface AMSend as UartSend[am_id_t id];
		interface Receive as UartReceive;
		interface Packet as UartPacket;
		interface AMPacket as UartAMPacket;
		
		interface AMSend as RadioSend[am_id_t id];
		interface Receive as RadioReceiveSensor;
		interface Receive as RadioReceiveRoute;
		interface Receive as RadioReceiveFault;
		interface Packet as RadioPacket;
		interface AMPacket as RadioAMPacket;
	}
}

implementation 
{
	/* Member vars */
	message_t  uartQueueBufs[UART_QUEUE_LEN];
	message_t  * ONE_NOK uartQueue[UART_QUEUE_LEN];
	uint8_t    uartIn;
	uint8_t    uartOut;
	bool       uartBusy;
	bool	   uartFull;
	
	message_t  radioQueueBufs[RADIO_QUEUE_LEN];
	message_t  * ONE_NOK radioQueue[RADIO_QUEUE_LEN];
	uint8_t    radioIn;
	uint8_t	   radioOut;
	bool       radioBusy;
	bool	   radioFull;
	
	am_addr_t  radio_dest; /* Where to send all radio messages */
	uint8_t	   last_cmd_id; /* Track what the last route message used was */
	uint16_t   current_distance; /* How many hops to sink */
	uint8_t    current_fault;
	
	/* Task prototypes */
	task void radioSendBroadcastTask();
	task void radioSendUnicastTask();
	task void uartSendTask();
	
	/* Helper for send */
	void radioSendTask(am_addr_t dst);
	
	void failBlink() 
	{
		call Leds.led2Toggle();
	}
	
	void dropBlink() 
	{
		call Leds.led2Toggle();
	}

	/* Initialization task */
	event void Boot.booted() 
	{
		uint8_t i;
		if(TOS_NODE_ID == BASE_STATION_NODE_ID)
		{

			for (i = 0; i < UART_QUEUE_LEN; i++)
			{
				uartQueue[i] = &uartQueueBufs[i];
			}
			uartIn = uartOut = 0;
			uartBusy = FALSE;
			uartFull = TRUE;
			
			if (call SerialControl.start() == EALREADY)
			{
				uartFull = FALSE;
			}
			dbg("Boot","Boot: Node %d serial started.\n", TOS_NODE_ID);
		}
		
		for (i = 0; i < RADIO_QUEUE_LEN; i++)
		{
			radioQueue[i] = &radioQueueBufs[i];
		}
		radioIn = radioOut = 0;
		radioBusy = FALSE;
		radioFull = TRUE;

		if (call RadioControl.start() == EALREADY)
		{
			radio_dest = BASE_STATION_NODE_ID;
			last_cmd_id = 0;
			current_distance = 0;
			radioFull = FALSE;
		}
		dbg("Boot","Boot: Node %d radio started.\n", TOS_NODE_ID);

		if(TOS_NODE_ID != BASE_STATION_NODE_ID)
		{
			call Timer0.startPeriodic(TIMER_PERIOD_MILLI);
			dbg("Boot","Boot: Node %d timer started.\n", TOS_NODE_ID);
		}
	}

	/* Timer0 periodic function */
	event void Timer0.fired() 
	{
		message_payload_t payload;
		payload.node_id = TOS_NODE_ID;
		
		/* Generate random sensor data 
		 * Limit to 0x0080 - 0x0FFF for normal behavior */
		payload.sensor_reading = call Random.rand16();
		if(!(current_fault & BAD_SENSOR_READINGS || current_fault & FAIL_SENSOR))
		{
			payload.sensor_reading &= 0x03FF;
			payload.sensor_reading |= 0x0080;
		}
		//dbg("Payload","Payload: %d\n", payload.sensor_reading);
		
		if(current_fault & FAIL_SENSOR)
		{
			payload.sensor_status = BIT_FAILED;
		}
		else
		{
			payload.sensor_status = BIT_OK;	
		}
		
		memcpy(radioQueueBufs[radioIn].data, &payload, sizeof(message_payload_t));
		call RadioPacket.setPayloadLength(&radioQueueBufs[radioIn], sizeof(message_payload_t));
		
		radioIn = (radioIn + 1) % RADIO_QUEUE_LEN;
		if (!radioBusy)
		{
			post radioSendUnicastTask();
			radioBusy = TRUE;
		}
	}
	
	/* Serial control functions */
	event void SerialControl.startDone(error_t error) 
	{
		if (error == SUCCESS) 
		{
			uartFull = FALSE;
		}
	}
	
	event void SerialControl.stopDone(error_t error) {}
	
	/* Only handles messages of type 1 */
	event message_t *UartReceive.receive(message_t *msg, void *payload, uint8_t len) 
	{
		message_t * ret = msg;
		network_command_t * net_cmd = (network_command_t *)payload;
		network_route_t route_cmd;

		len = call UartPacket.payloadLength(msg);
		dbg("Serial","Network command received (len %d)): %d %d\n", len, net_cmd->cmd_id, net_cmd->cmd_num);
		
		route_cmd.hops_to_sink = 0;
		route_cmd.cmd_id = net_cmd->cmd_num;
		call RadioAMPacket.setType(&radioQueueBufs[radioIn], NETWORK_TYPE);
		memcpy(radioQueueBufs[radioIn].data, &route_cmd, sizeof(network_route_t));
		call RadioPacket.setPayloadLength(&radioQueueBufs[radioIn], sizeof(network_route_t));
		
		radioIn = (radioIn + 1) % RADIO_QUEUE_LEN;
		if (!radioBusy)
		{
			post radioSendBroadcastTask();
			radioBusy = TRUE;
		}
		
		return ret;
	}
	
	task void uartSendTask() 
	{
		uint8_t len;
		am_id_t id;
		am_addr_t addr, src;
		message_t* msg;
		am_group_t grp;
		atomic
		{
			if (uartIn == uartOut && !uartFull)
			{
				uartBusy = FALSE;
				return;
			}
		}

		msg = uartQueue[uartOut];
		id = call RadioAMPacket.type(msg);
		len = call RadioPacket.payloadLength(msg);
		addr = call RadioAMPacket.destination(msg);
		src = call RadioAMPacket.source(msg);
		grp = call RadioAMPacket.group(msg);
		call UartPacket.clear(msg);
		call UartAMPacket.setSource(msg, src);
		call UartAMPacket.setGroup(msg, grp);
		
		dbg("Serial", "Uart message of len %d\n", len);

		if (call UartSend.send[id](addr, uartQueue[uartOut], len) == SUCCESS)
		{
			call Leds.led1Toggle();
		}
		else
		{
			failBlink();
			post uartSendTask();
		}
	}
	
	event void UartSend.sendDone[am_id_t id](message_t* msg, error_t error) 
	{
		if (error != SUCCESS)
		{
			failBlink();
		}
		else
		{
			atomic
			{
				if (msg == uartQueue[uartOut])
				{
					if (++uartOut >= UART_QUEUE_LEN)
					{
						uartOut = 0;
					}
					if (uartFull)
					{
						uartFull = FALSE;
					}
				}
			}
		}
		
		/* Keep calling until uart queue is empty */
		post uartSendTask();
	}
	
	/* Radio control functions */
	event void RadioControl.startDone(error_t error) 
	{
		if (error == SUCCESS) 
		{
			radioFull = FALSE;
		}
	}
	
	event void RadioControl.stopDone(error_t error) {}

	/* If it is sensor data, just forward it on over serial (if sink)
	 * or radio (elsewise).
	 */
	event message_t *RadioReceiveSensor.receive(message_t *msg, void *payload, uint8_t len) 
	{
		message_t *ret = msg;
		if(TOS_NODE_ID == BASE_STATION_NODE_ID)
		{
			/* Send message over serial */
			uartQueue[uartIn] = msg;
			uartIn = (uartIn + 1) % UART_QUEUE_LEN;
			if (!uartBusy)
			{
				post uartSendTask();
				uartBusy = TRUE;
			}
		}
		else
		{
			/* Forward message over radio */
			radioQueue[radioIn] = msg;
			radioIn = (radioIn + 1) % RADIO_QUEUE_LEN;
			if (!radioBusy)
			{
				post radioSendUnicastTask();
				radioBusy = TRUE;
			}
		}
		
		return ret;
	}
	
	event message_t *RadioReceiveFault.receive(message_t *msg, void *payload, uint8_t len) 
	{
		message_t *ret = msg;
		fault_command_t * fault_cmd = (fault_command_t *)payload;
		dbg("Fault", "Fault message received.\n");
		current_fault = fault_cmd->fault_type;
		return ret;
	}
	
	/* On a route message, we want to update the route to the closest node with
	 * the least number of hops to the sink.
	 */
	event message_t *RadioReceiveRoute.receive(message_t *msg, void *payload, uint8_t len) 
	{
		message_t *ret = msg;
		network_route_t * route_cmd = (network_route_t *)payload;
		uint8_t send_update = 0;
		dbg("Radio", "Route message received.\n");
		if(TOS_NODE_ID != BASE_STATION_NODE_ID)
		{
			/* If we get a new command to update the route,
			 * take the new route.
			 */
			if(route_cmd->cmd_id > last_cmd_id)
			{
				current_distance = route_cmd->hops_to_sink;
				radio_dest = call RadioAMPacket.source(msg);
				last_cmd_id = route_cmd->cmd_id;
				send_update = 1;
			}
			
			/* If we get a command with the same number,
			* only update if it is better than our current
			* number of hops
			*/
			else if(route_cmd->cmd_id == last_cmd_id &&
			 		route_cmd->hops_to_sink < current_distance)
			{
				current_distance = route_cmd->hops_to_sink;
				radio_dest = call RadioAMPacket.source(msg);
				send_update = 1;			 	
			}
			 
			/* Forward message over radio if it is new or the
			* number of hops is better*/
			if(send_update)
			{
				dbg("Route", "Updating route to %d (distance: %d).\n", radio_dest, current_distance);
				route_cmd->hops_to_sink = current_distance + 1;
				radioQueue[radioIn] = msg;
				radioIn = (radioIn + 1) % RADIO_QUEUE_LEN;
				if (!radioBusy)
				{
					post radioSendBroadcastTask();
					radioBusy = TRUE;
				}
			}
		}
		
		return ret;
	}
	
	task void radioSendBroadcastTask()
	{
		radioSendTask(AM_BROADCAST_ADDR); 
	}
	
	task void radioSendUnicastTask()
	{
		radioSendTask(radio_dest);
	}
	
	void radioSendTask(am_addr_t dst) 
	{
		uint8_t len;
		am_id_t id;
		am_addr_t source;
		message_t* msg;
		uint16_t feeling_lucky;
		
		/* If fail radio set, back off for a random
		 * amount of time before sending
		 */
		if(current_fault & FAIL_RADIO)
		{
			feeling_lucky = call Random.rand16();
			if(feeling_lucky % 4 != 0)
			{
				dbg("Fault", "Radio issues. Transmit failure %d.\n", feeling_lucky % 4);
				radioBusy = FALSE;
				return;	
			}
		}
		
		/* If fail battery set, dont respond ever */
		if(current_fault & FAIL_BATTERY)
		{
			radioBusy = FALSE;
			return;
		}

		atomic
		{
			if (radioIn == radioOut && !radioFull)
			{
				radioBusy = FALSE;
				return;
			}
		}

		msg = radioQueue[radioOut];
		source = (am_addr_t)TOS_NODE_ID;
		id = call RadioAMPacket.type(msg);
		len = call RadioPacket.payloadLength(msg);
		call RadioPacket.clear(msg);
		call RadioAMPacket.setSource(msg, source);
		
		if (call RadioSend.send[id](dst, msg, len) == SUCCESS)
		{
			call Leds.led0Toggle();
		}
		else
		{
			failBlink();
			if(dst == AM_BROADCAST_ADDR)
			{
				post radioSendBroadcastTask();
			}
			else
			{
				post radioSendUnicastTask();
			}
		}
	}

	event void RadioSend.sendDone[am_id_t id](message_t* msg, error_t error) 
	{
		if (error != SUCCESS)
		{
			failBlink();
		}
		else
		{
			atomic
			{
				if (msg == radioQueue[radioOut])
				{
					if (++radioOut >= RADIO_QUEUE_LEN)
					{
						radioOut = 0;
					}
					if (radioFull)
					{
						radioFull = FALSE;
					}
				}
			}
		}

		/* Keep calling until radio queue is empty */
	    if(call RadioAMPacket.destination(msg) == AM_BROADCAST_ADDR)
		{
			post radioSendBroadcastTask();
		}
		else
		{
			post radioSendUnicastTask();
		}
	}

}