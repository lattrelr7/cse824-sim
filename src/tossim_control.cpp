#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sf/tossim.h>
#include <sf/SerialPacket.h>
#include <sf/SerialForwarder.h>
#include <sf/Throttle.h>
#include <sf/sim_serial_forwarder.h>
#include <iostream>
#include <string>
#include "hm_message.h"

static void hexprint(uint8_t *packet, int len);
static void * InputHandlerThread(void * args);
static void * SerialListenThread(void * args);
static void InjectFault(Tossim * t, int node_addr, FAULT_TYPES fault);
static void SendNetworkCommand(Tossim * t, NETWORK_COMMAND_TYPES net_cmd_type);
static void LogTcDbg( const char * format, ... );

static int network_command_id = 1;

int main()
{
	unsigned int number_of_nodes = 1;

	Tossim* t = new Tossim(NULL);
	t->init();
	Throttle throttle = Throttle(t, 10);
	Radio* r = t->radio();
	SerialForwarder sf = SerialForwarder(9001);

	FILE * topology_file = fopen("topo.txt", "r");
	FILE * noise_file = fopen("noise.txt", "r");
	FILE * sim_dbg_out = fopen("sim_dbg_out.txt", "w");
	FILE * tc_dbg_out = fopen("tc_dbg_out.txt", "w"); /*Wipe log file*/
	fclose(tc_dbg_out);

	/* Read topology and set up radio*/
	double dbM;
	unsigned int radio_src;
	unsigned int radio_dst;
	std::cout << "Configuring radios...";
	while(fscanf(topology_file, "%d %d %lf\n", &radio_src, & radio_dst, &dbM) != EOF)
	{
		r->add(radio_src, radio_dst, dbM);
		if(radio_dst > number_of_nodes - 1)
		{
			number_of_nodes++;
		}
	}
	fclose(topology_file);
	std::cout << number_of_nodes << " nodes in simulation" << std::endl;

	/* Add TOSSIM channels (print dbg messages to stdout) */
	//t->addChannel((char *)"Serial", stdout);
	t->addChannel((char *)"Route", sim_dbg_out);
	t->addChannel((char *)"Fault", sim_dbg_out);
	t->addChannel((char *)"Boot", sim_dbg_out);

	/* Read noise */
	std::cout << "Configuring noise..." << std::endl;
	int noise_level;
	while(fscanf(noise_file, "%d\n", &noise_level) != EOF)
	{
		for(unsigned int i=0; i < number_of_nodes; i++)
		{
			t->getNode(i)->addNoiseTraceReading(noise_level);
		}
	}
	fclose(noise_file);

	std::cout << "Setting boot sequence and creating noise models..." << std::endl;
	for(unsigned int i=0; i < number_of_nodes; i++)
	{
		std::cout << "." << std::flush;
		t->getNode(i)->createNoiseModel();
		t->getNode(i)->bootAtTime((i * 0.1) * t->ticksPerSecond());
	}
	std::cout << std::endl;

	pthread_t input_thread;
	pthread_t serial_rx_thread;
	pthread_create(&input_thread, NULL, InputHandlerThread, t);
	pthread_create(&serial_rx_thread, NULL, SerialListenThread, NULL);

	std::cout << "Starting event loop..." << std::endl;

	sf.process();
	throttle.initialize();
	while(1)
	{
		throttle.checkThrottle();
		t->runNextEvent();
		sf.process();
	}
}

static void InjectFault(Tossim * t, int node_addr, FAULT_TYPES fault)
{
	fault_command_t fault_pkt;
	fault_pkt.fault_type = fault;

	Packet * radio_packet = t->newPacket();
	radio_packet->setData((char *)&fault_pkt, sizeof(fault_command_t));
	radio_packet->setLength(sizeof(fault_command_t));
	radio_packet->setType(FAULT_TYPE);
	radio_packet->setDestination(node_addr);
	radio_packet->deliverNow(node_addr);

	std::cout << "Sent fault " << fault << " to node " << node_addr << std::endl;
}

static void SendNetworkCommand(Tossim * t, NETWORK_COMMAND_TYPES net_cmd_type)
{
	network_command_t net_cmd;
	net_cmd.cmd_id = net_cmd_type;
	net_cmd.cmd_num = network_command_id;

	SerialPacket * serial_packet = t->newSerialPacket();
	serial_packet->setData((char *)&net_cmd, sizeof(network_command_t));
	serial_packet->setLength(sizeof(network_command_t));
	serial_packet->setType(NETWORK_TYPE);
	serial_packet->setDestination(0);
	serial_packet->deliverNow(0);

	std::cout << "Sent network command " << net_cmd_type << " (id " << network_command_id << ")" << std::endl;
	network_command_id = (network_command_id + 1) % 256;
}

static void * InputHandlerThread(void * args)
{
	Tossim* t = (Tossim *)args;
	char input_command[256];
	usleep(1000000);
	std::cout << "Ready" << std::endl;
	while(1)
	{
		int i_arg1;
		int i_arg2;
		char s_arg1[256];
		std::cout << ">>" << std::flush;
		std::cin.getline(input_command, 256);
		if(sscanf(input_command, "network %s", s_arg1) > 0)
		{
			SendNetworkCommand(t, UPDATE_ROUTE);
		}
		else if(sscanf(input_command, "fault %d %d", &i_arg1, &i_arg2) > 0)
		{
			InjectFault(t, i_arg1, (FAULT_TYPES)i_arg2);
		}
		else
		{
			std::cout << "Unknown command: " << input_command << std::endl;
			std::cout << "---------------------------------------------" << std::endl;
			std::cout << "Options:" << std::endl;
			std::cout << "\tnetwork update\t\tSend a routing update request" << std::endl;
			std::cout << "\tfault [node] [type]\tInject a fault into the simulation" << std::endl;
			std::cout << std::endl;
		}
	}

	return NULL;
}

static void hexprint(uint8_t *packet, int len)
{
	for (int i = 0; i < len; i++)
	{
		LogTcDbg("%02x ", packet[i]);
		if(i != 0 && ((i+1) % 4) == 0)
		{
			LogTcDbg("\n");
		}
	}
	LogTcDbg("\n");
}

static void * SerialListenThread(void * args)
{
	int fd = sim_sf_open_source("127.0.0.1", 9001);
	if (fd < 0)
	{
		LogTcDbg("Couldn't open serial forwarder\n");
		exit(1);
	}

	int len;
	while(1)
	{
		uint8_t * packet = (uint8_t *)sim_sf_read_packet(fd, &len);
		if(len != sizeof(serial_header_t) + sizeof(message_payload_t))
		{
			LogTcDbg("Rxed unknown message type!\n");
			LogTcDbg("%d != (%lu + %lu)\n", len,
					sizeof(serial_header_t),
					sizeof(message_payload_t));
		}
		else if(!packet)
		{
			LogTcDbg("Failed to rx packet\n");
		}
		else
		{
			serial_header_t * header = (serial_header_t *)packet;
			message_payload_t * payload = (message_payload_t *)(packet + sizeof(serial_header_t));
			//hexprint((uint8_t*)packet, len);
			LogTcDbg("*********NEW HM MESSAGE*********\n");
			LogTcDbg("---------HEADER-----------------\n");
			LogTcDbg("Source Addr: %d\n", header->src);
			LogTcDbg("Dest Addr: %d\n", header->dest);
			LogTcDbg("Group: %d\n", header->group);
			LogTcDbg("Type: %d\n", header->type);
			LogTcDbg("Payload Length: %d\n", header->length);
			LogTcDbg("---------PAYLOAD----------------\n");
			LogTcDbg("Node ID: %d\n", ntohs(payload->node_id));
			LogTcDbg("********************************\n");
		}
		free((void *)packet);
	}

	return NULL;
}

static void LogTcDbg( const char * format, ... )
{
	FILE * tc_dbg_out = fopen("tc_dbg_out.txt", "a");
    va_list argptr;
    va_start(argptr, format);
    vfprintf(tc_dbg_out, format, argptr);
    va_end(argptr);
    fclose(tc_dbg_out);
}