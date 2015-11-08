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
		interface Timer<TMilli> as Timer1;
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
		interface Receive as RadioReceiveAlive;
		//interface Receive as RadioReceiveSnoop[am_id_t id];
		interface Packet as RadioPacket;
		interface AMPacket as RadioAMPacket;
	}
}

implementation 
{
	/* Member vars */
	message_t  uartQueueBufs[UART_QUEUE_LEN];
	uint8_t    uartIn;
	uint8_t    uartOut;
	bool       uartBusy;
	bool	   uartFull;
	
	message_t  radioQueueBufs[RADIO_QUEUE_LEN];
	uint8_t    radioIn;
	uint8_t	   radioOut;
	bool       radioBusy;
	bool	   radioFull;
	
	bool 	   rerouting; /* Are we sending to 1 or 2? */
	am_addr_t  radio_dest2; /* Where to go if dest is down */
	am_addr_t  radio_dest; /* Where to send all radio messages */
	uint8_t	   last_cmd_id; /* Track what the last route message used was */
	uint16_t   current_distance; /* How many hops to sink */
	uint8_t    injected_fault; /* Fault injection for simulation only */
	
	neighbor_t neighbors[MAX_NEIGHBORS];
	neighbor_info_t info_queue[INFO_QUEUE_LEN];
	uint8_t		infoIn;
	uint8_t		infoOut;
	
	/* Task prototypes */
	task void radioSendTask();
	task void uartSendTask();
	
	/* Helper for send */
	void radioAddUnicast(am_addr_t dst, uint8_t type, void * payload, uint8_t payload_size);
	void radioAddBroadcast(uint8_t type, void * payload, uint8_t payload_size);
	void serialAdd(uint8_t type, void * payload, uint8_t payload_size);
	
	/* Neighbor maintenance */
	void decrement_neighbors();
	void update_neighbor(uint16_t node_id, uint8_t hops_to_sink);
	
	void failBlink() {call Leds.led2Toggle();}
	void dropBlink() {call Leds.led2Toggle();}

	/* Initialization task */
	event void Boot.booted() 
	{
		if(TOS_NODE_ID == BASE_STATION_NODE_ID)
		{
			uartIn = uartOut = 0;
			uartBusy = FALSE;
			uartFull = TRUE;
			
			if (call SerialControl.start() == EALREADY)
			{
				uartFull = FALSE;
			}
			dbg("Boot","Boot: Node %d serial started.\n", TOS_NODE_ID);
		}
		
		radioIn = radioOut = 0;
		radioBusy = FALSE;
		radioFull = TRUE;
		
		infoIn = infoOut = 0;

		if (call RadioControl.start() == EALREADY)
		{
			radio_dest = BASE_STATION_NODE_ID;
			radio_dest2 = BASE_STATION_NODE_ID;
			last_cmd_id = 0;
			current_distance = 0;
			rerouting = FALSE;
			radioFull = FALSE;
		}
		
		dbg("Boot","Boot: Node %d radio started.\n", TOS_NODE_ID);

		if(TOS_NODE_ID != BASE_STATION_NODE_ID)
		{
			call Timer0.startPeriodic(SENSE_PERIOD_MILLI);
			call Timer1.startPeriodic(ALIVE_PERIOD_MILLI);
			dbg("Boot","Boot: Node %d timer started.\n", TOS_NODE_ID);
		}
	}

	/* Timer0 periodic function - 'read' sensors */
	event void Timer0.fired() 
	{
		uint16_t sensor_data;
		
		/* Generate random sensor data 
		 * Limit to 0x0080 - 0x0FFF for normal behavior */
		sensor_data = call Random.rand16();
		sensor_data &= 0x03FF;
		sensor_data |= 0x0080;
		//dbg("Payload","Payload: %d\n", payload.sensor_reading);
		
		/* Check if we should send extended message or not */
		if(infoIn != infoOut)
		{
			ext_message_payload_t payload;
			payload.node_id = TOS_NODE_ID;
			payload.sensor_data = sensor_data;
			payload.info_type = info_queue[infoOut].info_type;
			payload.info_addr = info_queue[infoOut].info_addr;
			radioAddUnicast(radio_dest, EXT_TYPE, (void *)&payload, sizeof(ext_message_payload_t));
			
			if (++infoOut >= INFO_QUEUE_LEN)
			{
				infoOut = 0;
			}
		}
		else
		{
			message_payload_t payload;
			payload.node_id = TOS_NODE_ID;
			payload.sensor_data = sensor_data;
			radioAddUnicast(radio_dest, SENSOR_TYPE, (void *)&payload, sizeof(message_payload_t));
		}
	}
	
	/* Timer1 periodic function - broadcast alive message*/
	event void Timer1.fired() 
	{
		alive_message_t alive_data;
		alive_data.hops_to_sink = current_distance;
		
		decrement_neighbors();
		
		//broadcast message
		if(!(injected_fault & (FAIL_ALIVE | FAIL_BATTERY)))
			radioAddBroadcast(ALIVE_TYPE, (void *)&alive_data, sizeof(alive_message_t));
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
	
	/* Only handles messages of NETWORK_TYPE */
	event message_t *UartReceive.receive(message_t *msg, void *payload, uint8_t len) 
	{
		message_t * ret = msg;
		network_command_t * net_cmd = (network_command_t *)payload;
		network_route_t route_cmd;

		len = call UartPacket.payloadLength(msg);
		dbg("Serial","Network command received (len %d)): %d %d\n", len, net_cmd->cmd_id, net_cmd->cmd_num);
		
		route_cmd.hops_to_sink = 0;
		route_cmd.cmd_id = net_cmd->cmd_num;
		
		radioAddBroadcast(NETWORK_TYPE, (void *)&route_cmd, sizeof(network_route_t));
		
		return ret;
	}
	
	task void uartSendTask() 
	{
		uint8_t len;
		am_id_t id;
		am_addr_t dst;
		message_t* msg;
		
		atomic
		{
			if (uartIn == uartOut && !uartFull)
			{
				uartBusy = FALSE;
				return;
			}
		}

		msg = &uartQueueBufs[uartOut];
		id = call UartAMPacket.type(msg);
		len = call UartPacket.payloadLength(msg);
		dst = call UartAMPacket.destination(msg);
		
		dbg("Serial", "Uart message of len %d\n", len);

		if (call UartSend.send[id](dst, msg, len) == SUCCESS)
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
				if (msg == &uartQueueBufs[uartOut])
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
		uint8_t id = call RadioAMPacket.type(msg);
		if(TOS_NODE_ID == BASE_STATION_NODE_ID)
		{
			serialAdd(id, payload, len);
		}
		else
		{
			radioAddUnicast(radio_dest, id, payload, len);
		}
		
		return ret;
	}
	
	/*************************************************************************************
	 * event message_t *RadioReceiveFault.receive(message_t *msg, void *payload, uint8_t len) 
	 * 
	 * 
	 * 
	 *************************************************************************************/
	event message_t *RadioReceiveFault.receive(message_t *msg, void *payload, uint8_t len) 
	{
		message_t *ret = msg;
		fault_command_t * fault_cmd = (fault_command_t *)payload;
		dbg("Fault", "Fault message received.\n");
		injected_fault = fault_cmd->fault_type;
		return ret;
	}
	
	/*************************************************************************************
	 * event message_t *RadioReceiveAlive.receive(message_t *msg, void *payload, uint8_t len) 
	 * 
	 * 
	 * 
	 *************************************************************************************/
	event message_t *RadioReceiveAlive.receive(message_t *msg, void *payload, uint8_t len) 
	{
		message_t *ret = msg;
		dbg("Alive", "Alive message received from %d.\n", call RadioAMPacket.source(msg));
		update_neighbor(call RadioAMPacket.source(msg), ((alive_message_t *)payload)->hops_to_sink);
		return ret;
	}

	/* On a route message, we want to update the route to the closest node with
	 * the least number of hops to the sink.
	 */
	/*************************************************************************************
	 * event message_t *RadioReceiveRoute.receive(message_t *msg, void *payload, uint8_t len) 
	 * 
	 * 
	 * 
	 *************************************************************************************/	 
	event message_t *RadioReceiveRoute.receive(message_t *msg, void *payload, uint8_t len) 
	{
		message_t *ret = msg;
		network_route_t * route_cmd = (network_route_t *)payload;
		uint8_t send_update = 0;
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
				
				radioAddBroadcast(NETWORK_TYPE, (void *)route_cmd, sizeof(network_route_t));
			}
		}
		
		return ret;
	}
	
	task void radioSendTask() 
	{
		uint8_t len;
		am_id_t id;
		am_addr_t dst;
		message_t* msg;

		atomic
		{
			if (radioIn == radioOut && !radioFull)
			{
				radioBusy = FALSE;
				return;
			}
		}

		msg = &radioQueueBufs[radioOut];
		id = call RadioAMPacket.type(msg);
		len = call RadioPacket.payloadLength(msg);
		dst = call RadioAMPacket.destination(msg);
		
		//dbg("Route", "Sending from %d to %d.\n", call RadioAMPacket.source(msg), dst);
		
		if (call RadioSend.send[id](dst, msg, len) == SUCCESS)
		{
			call Leds.led0Toggle();
		}
		else
		{
			failBlink();
			post radioSendTask();
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
				if (msg == &radioQueueBufs[radioOut])
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
		post radioSendTask();
	}
	
	void radioAddUnicast(am_addr_t dst, uint8_t type, void * payload, uint8_t payload_size)
	{
		if(injected_fault & (FAIL_DATA | FAIL_BATTERY))
			return;
			
		call RadioAMPacket.setType(&radioQueueBufs[radioIn], type);
		call RadioAMPacket.setDestination(&radioQueueBufs[radioIn], dst);
		call RadioAMPacket.setSource(&radioQueueBufs[radioIn], TOS_NODE_ID);
		call RadioPacket.setPayloadLength(&radioQueueBufs[radioIn], payload_size);
		
		memcpy(radioQueueBufs[radioIn].data, payload, payload_size);
		
		radioIn = (radioIn + 1) % RADIO_QUEUE_LEN;
		if (!radioBusy)
		{
			post radioSendTask();
			radioBusy = TRUE;
		}
	}
	
	void radioAddBroadcast(uint8_t type, void * payload, uint8_t payload_size)
	{
		radioAddUnicast(AM_BROADCAST_ADDR, type, payload, payload_size);
	}
	
	void serialAdd(uint8_t type, void * payload, uint8_t payload_size)
	{
		/* Send message over serial */
		call UartAMPacket.setType(&uartQueueBufs[uartIn], type);
		call UartAMPacket.setDestination(&uartQueueBufs[uartIn], 0x00);
		call UartAMPacket.setSource(&uartQueueBufs[uartIn], TOS_NODE_ID);
		call UartAMPacket.setGroup(&uartQueueBufs[uartIn], 0);
		call UartPacket.setPayloadLength(&uartQueueBufs[uartIn], payload_size);
		
		memcpy(uartQueueBufs[uartIn].data, payload, payload_size);
		
		uartIn = (uartIn + 1) % UART_QUEUE_LEN;
		if (!uartBusy)
		{
			post uartSendTask();
			uartBusy = TRUE;
		}
	}
	
	void decrement_neighbors()
	{	
		unsigned int i;
		for(i=0; i < MAX_NEIGHBORS; i++)
		{
			if(neighbors[i].node_id && neighbors[i].count != 0)
			{
				neighbors[i].count -= ALIVE_DEC_BY;
				
				if(neighbors[i].count == 0)
				{
					info_queue[infoIn].info_type = LOST_NODE;
					info_queue[infoIn].info_addr = neighbors[i].node_id;
					infoIn = (infoIn + 1) % INFO_QUEUE_LEN;
					dbg("Alive", "Lost contact with %d!.\n", neighbors[i].node_id);
				}
			}
		}
	}
	
	void update_neighbor(am_addr_t node_id, uint8_t hops_to_sink)
	{
		unsigned int i;
		unsigned int found = 0;
		for(i=0; i < MAX_NEIGHBORS; i++)
		{
			if(neighbors[i].node_id == node_id)
			{
				found = 1;
				neighbors[i].hops_to_sink = hops_to_sink;
				
				if(neighbors[i].count == 0)
				{
					info_queue[infoIn].info_type = FOUND_NODE;
					info_queue[infoIn].info_addr = neighbors[i].node_id;
					infoIn = (infoIn + 1) % INFO_QUEUE_LEN;
					dbg("Alive", "Rediscovered %d!.\n", neighbors[i].node_id);
					
					neighbors[i].count = MAX_ALIVE_COUNT;
				}
				
				if(neighbors[i].count != MAX_ALIVE_COUNT)
				{
					neighbors[i].count += 1;
				}
				
				break;
			}
		}
		
		if(!found)
		{
			for(i=0; i < MAX_NEIGHBORS; i++)
			{
				if(!neighbors[i].node_id)
				{
					info_queue[infoIn].info_type = FOUND_NODE;
					info_queue[infoIn].info_addr = node_id;
					infoIn = (infoIn + 1) % INFO_QUEUE_LEN;
					dbg("Alive", "Discovered %d.\n", node_id);
					
					neighbors[i].node_id = node_id;
					neighbors[i].count = MAX_ALIVE_COUNT;
					neighbors[i].hops_to_sink = hops_to_sink;
					break;
				}
			}			
		}
	}

}