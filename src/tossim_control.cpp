#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#include <sqlite3.h>
#include <iostream>
#include <string>
#include <map>
#include "hm_message.h"
#include "synchronized_queue.h"
#include "NodeModel.h"

static void hexprint(uint8_t *packet, int len);
static void * InputHandlerThread(void * args);
static void * SerialListenThread(void * args);
static void * SQLHandlerThread(void * args);
static void * UpdateAndLogNodeModel(void * args);
static void InjectFault(Tossim * t, int node_addr, uint8_t fault);
static void SendNetworkCommand(Tossim * t, NETWORK_COMMAND_TYPES net_cmd_type);
static void LogTcMsg( const char * format, ... );
static void PrintOptions();
static void UpdateFaultModel(uint8_t * packet);

static int network_command_id = 1;
static SynchronizedQueue<message_payload_t> my_queue;
static sqlite3 *db;
static std::map<unsigned int, NodeModel> node_models;

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
	FILE * powertossimz = fopen("Energy.txt", "w");
	FILE * tc_dbg_out = fopen("tc_dbg_out.txt", "w"); /*Wipe log file*/
	FILE * tc_model_out = fopen("tc_model_out.txt", "w"); /*Wipe log file*/
	fclose(tc_dbg_out);
	fclose(tc_model_out);

	
	int rc = sqlite3_open("824Sim.db", &db);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

	
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
	t->addChannel((char *)"Fault", sim_dbg_out);
	//t->addChannel((char *)"Radio", sim_dbg_out);
	t->addChannel((char *)"Alive", sim_dbg_out);
	t->addChannel((char *)"Broadcast", sim_dbg_out);
	t->addChannel((char *)"ENERGY_HANDLER", powertossimz);

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

	pthread_t input_thread, serial_rx_thread, sql_handle_thread, model_thread;

	pthread_create(&input_thread, NULL, InputHandlerThread, t);
	pthread_create(&serial_rx_thread, NULL, SerialListenThread, NULL);
	pthread_create(&sql_handle_thread, NULL, SQLHandlerThread, NULL);
	pthread_create(&model_thread, NULL, UpdateAndLogNodeModel, NULL);
	

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

static void InjectFault(Tossim * t, int node_addr, uint8_t fault)
{
	fault_command_t fault_pkt;
	fault_pkt.fault_type = fault;

	Packet * radio_packet = t->newPacket();
	radio_packet->setData((char *)&fault_pkt, sizeof(fault_command_t));
	radio_packet->setLength(sizeof(fault_command_t));
	radio_packet->setType(FAULT_TYPE);
	radio_packet->setDestination(node_addr);
	radio_packet->deliverNow(node_addr);

	std::cout << "Sent fault " << (int)fault << " to node " << node_addr << std::endl;
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
	PrintOptions();
	std::cout << "Ready" << std::endl;
	while(1)
	{
		int i_arg1;
		char s_arg1[256];
		std::cout << ">>" << std::flush;
		std::cin.getline(input_command, 256);
		if(sscanf(input_command, "network %s", s_arg1) > 0)
		{
			SendNetworkCommand(t, UPDATE_ROUTE);
		}
		else if(sscanf(input_command, "fault %d %s", &i_arg1, s_arg1) > 0)
		{
			if(strncmp(s_arg1, "CLEAR", strlen("CLEAR")) == 0)
			{
				InjectFault(t, i_arg1, 0x00);
			}
			else if(strncmp(s_arg1, "ALIVE", strlen("ALIVE")) == 0)
			{
				InjectFault(t, i_arg1, 0x02);
			}
			else if(strncmp(s_arg1, "DATA", strlen("DATA")) == 0)
			{
				InjectFault(t, i_arg1, 0x04);
			}
			else if(strncmp(s_arg1, "BATTERY", strlen("BATTERY")) == 0)
			{
				InjectFault(t, i_arg1, 0x08);
			}
		}
		else
		{
			std::cout << "Unknown command: " << input_command << std::endl;
			PrintOptions();
		}
	}

	return NULL;
}

static void PrintOptions()
{
	std::cout << "---------------------------------------------" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "\tnetwork update\t\tSend a routing update request" << std::endl;
	std::cout << "\tfault [node] [CLEAR|ALIVE|DATA|BATTERY]\tInject a fault into the simulation" << std::endl;
	std::cout << std::endl;
}

static void hexprint(uint8_t *packet, int len)
{
	for (int i = 0; i < len; i++)
	{
		LogTcMsg("%02x ", packet[i]);
		if(i != 0 && ((i+1) % 4) == 0)
		{
			LogTcMsg("\n");
		}
	}
	LogTcMsg("\n");
}

static void * SerialListenThread(void * args)
{
	int fd = sim_sf_open_source("127.0.0.1", 9001);
	if (fd < 0)
	{
		LogTcMsg("Couldn't open serial forwarder\n");
		exit(1);
	}

	int len;
	while(1)
	{
		uint8_t * packet = (uint8_t *)sim_sf_read_packet(fd, &len);
		if(!packet)
		{
			LogTcMsg("Failed to rx packet\n");
		}
		else
		{
			UpdateFaultModel(packet);

			serial_header_t * header = (serial_header_t *)packet;

			if(header->type == SENSOR_TYPE || header->type == EXT_TYPE)
			{
				message_payload_t * payload = (message_payload_t *)(packet + sizeof(serial_header_t));
				//hexprint((uint8_t*)packet, len);
				LogTcMsg("*********NEW BASE MESSAGE*********\n");
				/*LogTcMsg("---------HEADER-----------------\n");
				LogTcMsg("Source Addr: %d\n", header->src);
				LogTcMsg("Dest Addr: %d\n", header->dest);
				LogTcMsg("Group: %d\n", header->group);
				LogTcMsg("Type: %d\n", header->type);
				LogTcMsg("Payload Length: %d\n", header->length);*/
				LogTcMsg("---------PAYLOAD----------------\n");
				LogTcMsg("Node ID: %d\n", ntohs(payload->node_id));
				LogTcMsg("Next hop: %d\n", ntohs(payload->next_hop));
				LogTcMsg("Sensor Data: %d\n", ntohs(payload->sensor_data));
				LogTcMsg("Voltage: %d\n", ntohs(payload->voltage));
				LogTcMsg("********************************\n");

				my_queue.Enqueue(*payload);
			}
			if(header->type == EXT_TYPE)
			{
				ext_message_payload_t * payload = (ext_message_payload_t *)(packet + sizeof(serial_header_t));
				LogTcMsg("*********EXT MESSAGE*********\n");
				LogTcMsg("Node ID: %d\n", ntohs(payload->node_id));
				LogTcMsg("Info Type: %d\n", payload->info_type);
				LogTcMsg("Info Addr: %d\n", ntohs(payload->info_addr));
				LogTcMsg("********************************\n");
			}
			if(header->type == INFO_ONLY)
			{
				info_payload_t * payload = (info_payload_t *)(packet + sizeof(serial_header_t));
				LogTcMsg("*********INFO MSG***********\n");
				LogTcMsg("Node ID: %d\n", ntohs(payload->node_id));
				LogTcMsg("Info Type: %d\n", payload->info_type);
				LogTcMsg("Info Addr: %d\n", ntohs(payload->info_addr));
				LogTcMsg("********************************\n");
			}
		}
		free((void *)packet);
	}

	return NULL;
}

static void LogTcMsg( const char * format, ... )
{
	FILE * tc_dbg_out = fopen("tc_dbg_out.txt", "a");
    va_list argptr;
    va_start(argptr, format);
    vfprintf(tc_dbg_out, format, argptr);
    va_end(argptr);
    fclose(tc_dbg_out);
}


static void * SQLHandlerThread(void * args)
{
	const char insertTemplate[] = "INSERT INTO FaultMessages (NODEID,PARAM2,PARAM3,PARAM4,PARAM5) VALUES(%d, %d, %d, %d, %d);";
	// Loop forever
	int rc = 0;
	char *err_msg = 0;
	while(1){
		message_payload_t payload = my_queue.Dequeue();
		char query[128];
		sprintf(query, insertTemplate, ntohs(payload.node_id), ntohs(payload.sensor_data), ntohs(payload.voltage), 0, 0);
		rc = sqlite3_exec(db, query, 0, 0, &err_msg);
		
		if (rc != SQLITE_OK ) {
			printf("SQL error: %s\nUsing SQL Query %s\n", err_msg, query);
			sqlite3_free(err_msg);        
			sqlite3_close(db);
			break;
		}
	}
	
	sqlite3_close(db);
	return NULL;
}

static void * UpdateAndLogNodeModel(void * args)
{
	while(1)
	{
		FILE * tc_model_out = fopen("tc_model_out.txt", "w");
		std::map<unsigned int, NodeModel>::iterator it;
		for (it = node_models.begin(); it != node_models.end(); it++)
		{
			it->second.UpdateNodeState();
			fprintf(tc_model_out, "%s", it->second.PrintNode().c_str());
		}
		fclose(tc_model_out);
		sleep(2);
	}

    return NULL;
}

static void UpdateFaultModel(uint8_t * packet)
{
	serial_header_t * header = (serial_header_t *)packet;

	if(header->type == SENSOR_TYPE || header->type == EXT_TYPE)
	{
		message_payload_t * payload = (message_payload_t *)(packet + sizeof(serial_header_t));

		//Create node models if they don't exist yet
		if(!node_models.count(ntohs(payload->next_hop)))
		{
			node_models[ntohs(payload->next_hop)] = NodeModel(ntohs(payload->next_hop));
		}
		if(!node_models.count(ntohs(payload->node_id)))
		{
			node_models[ntohs(payload->node_id)] = NodeModel(ntohs(payload->node_id));
		}

		NodeModel * current_node = &node_models[ntohs(payload->node_id)];
		current_node->UpdateParent(&node_models[ntohs(payload->next_hop)]);
		current_node->UpdateBatteryData(ntohs(payload->voltage));
		current_node->UpdateSensorData(ntohs(payload->sensor_data));
	}

	if(header->type == EXT_TYPE)
	{
		ext_message_payload_t * payload = (ext_message_payload_t *)(packet + sizeof(serial_header_t));

		//Create node models if they don't exist yet
		if(!node_models.count(ntohs(payload->info_addr)))
		{
			node_models[ntohs(payload->info_addr)] = NodeModel(ntohs(payload->info_addr));
		}

		NodeModel  * current_node = &node_models[ntohs(payload->node_id)];
		current_node->UpdateNeighborHeard(&node_models[ntohs(payload->info_addr)], (INFO_TYPES)payload->info_type);
		(&node_models[ntohs(payload->info_addr)])->UpdateNeighborHeardBy(current_node, (INFO_TYPES)payload->info_type);
	}

	if(header->type == INFO_ONLY)
	{
		info_payload_t * payload = (info_payload_t *)(packet + sizeof(serial_header_t));

		//Create node models if they don't exist yet
		if(!node_models.count(ntohs(payload->info_addr)))
		{
			node_models[ntohs(payload->info_addr)] = NodeModel(ntohs(payload->info_addr));
		}

		NodeModel  * current_node = &node_models[ntohs(payload->node_id)];
		current_node->UpdateNeighborHeard(&node_models[ntohs(payload->info_addr)], (INFO_TYPES)payload->info_type);
		(&node_models[ntohs(payload->info_addr)])->UpdateNeighborHeardBy(current_node, (INFO_TYPES)payload->info_type);
	}
}
