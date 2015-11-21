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
#include <time.h>
#include <sqlite3.h>
#include <iostream>
#include <string>
#include <map>
#include "hm_message.h"
#include "synchronized_queue.h"
#include "NodeModel.h"

#define TIME_LIMIT 1800

static void hexprint(uint8_t *packet, int len);
static void * InputHandlerThread(void * args);
static void * SerialListenThread(void * args);
static void UpdateDbFaultData(int nodeId, int value);
static void * UpdateAndLogNodeModel(void * args);
static void InjectFault(Tossim * t, int node_addr, uint8_t fault);
static void SendNetworkCommand(Tossim * t, NETWORK_COMMAND_TYPES net_cmd_type);
static void LogTcMsg( const char * format, ... );
static void PrintOptions();
static void UpdateFaultModel(uint8_t * packet);
static void ProcessInfo(NodeModel * node, INFO_TYPES info_type, uint16_t info_value);
static NodeModel * GetModel(unsigned int node_id);
static unsigned long long int GetTime();

static int network_command_id = 1;
static SynchronizedQueue<message_payload_t> my_queue;
static sqlite3 *db;
static std::map<unsigned int, NodeModel> node_models;
static long long unsigned int data_rxed = 0;
static long long unsigned int hm_rxed = 0;
static long long unsigned int start_time = 0;

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
	FILE * data_metrics = fopen("Data.txt", "w"); /*Wipe log file*/
	FILE * tc_dbg_out = fopen("tc_dbg_out.txt", "w"); /*Wipe log file*/
	FILE * tc_model_out = fopen("tc_model_out.txt", "w"); /*Wipe log file*/
	fclose(data_metrics);
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

	pthread_t input_thread, serial_rx_thread, model_thread;

	pthread_create(&input_thread, NULL, InputHandlerThread, t);
	pthread_create(&serial_rx_thread, NULL, SerialListenThread, NULL);
	pthread_create(&model_thread, NULL, UpdateAndLogNodeModel, NULL);
	

	std::cout << "Starting event loop..." << std::endl;

	start_time = GetTime();

	sf.process();
	throttle.initialize();
	while(1)
	{
		throttle.checkThrottle();
		t->runNextEvent();
		sf.process();

		if(TIME_LIMIT && (GetTime() - start_time > TIME_LIMIT))
		{
			break;
		}
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
		else if(strncmp(input_command, "print", strlen("print")) == 0)
		{
			std::cout << "*****NETWORK TOPOLOGY******" << std::endl;
			std::cout << GetModel(0)->PrintTopology(0) << std::endl;
		}
		else if(strncmp(input_command, "data", strlen("data")) == 0)
		{
			std::cout << "*****DATA METRICS******" << std::endl;
			std::cout << "Uptime (seconds): " << GetTime() - start_time << std::endl;
			std::cout << "Total radio data rxed (bytes): " << data_rxed << std::endl;
			std::cout << "HM only data rxed (bytes): " << hm_rxed << std::endl;
			std::cout << "\% HM data: " << ((float)hm_rxed/(float)data_rxed) * 100 << std::endl;
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
	std::cout << "\tprint\t\t\tPrint out topology starting at sink" << std::endl;
	std::cout << "\tdata\t\t\tData metrics" << std::endl;
	std::cout << "\tfault [node] [CLEAR|ALIVE|DATA|BATTERY]\tInject a fault" << std::endl;
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
				LogTcMsg("Sensor Data: %d\n", ntohs(payload->sensor_data));
				LogTcMsg("********************************\n");

				data_rxed += 5; //(TTL + node id + sensor data)

				UpdateDbFaultData(ntohs(payload->node_id), ntohs(payload->sensor_data));
			}
			if(header->type == EXT_TYPE || header->type == EXT2_TYPE || header->type == EXT3_TYPE || header->type == EXT4_TYPE)
			{
				ext_message_payload_t * payload = (ext_message_payload_t *)(packet + sizeof(serial_header_t));
				LogTcMsg("*********EXT MESSAGE*********\n");
				LogTcMsg("Node ID: %d\n", ntohs(payload->node_id));
				LogTcMsg("Info Type: %d\n", payload->info_type);
				LogTcMsg("Info Value: %d\n", ntohs(payload->info_value));
				LogTcMsg("********************************\n");

				data_rxed += 3;
				hm_rxed += 3; //(TTL + node id + sensor data)
			}
			if(header->type == EXT2_TYPE || header->type == EXT3_TYPE || header->type == EXT4_TYPE)
			{
				ext2_message_payload_t * payload = (ext2_message_payload_t *)(packet + sizeof(serial_header_t));
				LogTcMsg("*********EXT2 MESSAGE*********\n");
				LogTcMsg("Node ID: %d\n", ntohs(payload->node_id));
				LogTcMsg("Info Type: %d\n", payload->info_type2);
				LogTcMsg("Info Value: %d\n", ntohs(payload->info_value2));
				LogTcMsg("********************************\n");

				data_rxed += 3;
				hm_rxed += 3; //(TTL + node id + sensor data)
			}
			if(header->type == EXT3_TYPE || header->type == EXT4_TYPE)
			{
				ext3_message_payload_t * payload = (ext3_message_payload_t *)(packet + sizeof(serial_header_t));
				LogTcMsg("*********EXT3 MESSAGE*********\n");
				LogTcMsg("Node ID: %d\n", ntohs(payload->node_id));
				LogTcMsg("Info Type: %d\n", payload->info_type3);
				LogTcMsg("Info Value: %d\n", ntohs(payload->info_value3));
				LogTcMsg("********************************\n");

				data_rxed += 3;
				hm_rxed += 3; //(TTL + node id + sensor data)
			}
			if(header->type == EXT4_TYPE)
			{
				ext4_message_payload_t * payload = (ext4_message_payload_t *)(packet + sizeof(serial_header_t));
				LogTcMsg("*********EXT4 MESSAGE*********\n");
				LogTcMsg("Node ID: %d\n", ntohs(payload->node_id));
				LogTcMsg("Info Type: %d\n", payload->info_type4);
				LogTcMsg("Info Value: %d\n", ntohs(payload->info_value4));
				LogTcMsg("********************************\n");

				data_rxed += 3;
				hm_rxed += 3; //(TTL + node id + sensor data)
			}
			if(header->type == INFO_ONLY)
			{
				info_payload_t * payload = (info_payload_t *)(packet + sizeof(serial_header_t));
				LogTcMsg("*********INFO MSG***********\n");
				LogTcMsg("Node ID: %d\n", ntohs(payload->node_id));
				LogTcMsg("Info Type: %d\n", payload->info_type);
				LogTcMsg("Info Value: %d\n", ntohs(payload->info_value));
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


static void UpdateDbFaultData(int nodeId, int value)
{
	const char insertTemplate[] = "INSERT INTO FaultMessages (NodeId,Value) VALUES(%d,%d);";
	int rc = 0;
	char *err_msg = 0;
	char query[128];
	sprintf(query, insertTemplate, nodeId, value);
	rc = sqlite3_exec(db, query, 0, 0, &err_msg);

	if (rc != SQLITE_OK )
	{
		printf("SQL error: %s\nUsing SQL Query %s\n", err_msg, query);
		sqlite3_free(err_msg);
	}
}

static void * UpdateAndLogNodeModel(void * args)
{


	while(1)
	{
		FILE * tc_model_out = fopen("tc_model_out.txt", "w");
		std::map<unsigned int, NodeModel>::iterator it;
		for (it = node_models.begin(); it != node_models.end(); it++)
		{
			if(it->second.UpdateNodeState())
			{
				it->second.UpdateDb(db);
			}
			fprintf(tc_model_out, "%s", it->second.PrintNode().c_str());
		}
		fclose(tc_model_out);

		FILE * data_metrics = fopen("Data.txt", "a");
		fprintf(data_metrics, "%llu,%llu,%llu,%f\n",
				GetTime() - start_time,
				data_rxed,
				hm_rxed,
				((float)hm_rxed/(float)data_rxed) * 100);
		fclose(data_metrics);

		sleep(2);
	}

    return NULL;
}

static void UpdateFaultModel(uint8_t * packet)
{
	serial_header_t * header = (serial_header_t *)packet;

	if(header->type == SENSOR_TYPE ||
	   header->type == EXT_TYPE ||
	   header->type == EXT2_TYPE ||
	   header->type == EXT3_TYPE ||
	   header->type == EXT4_TYPE)
	{
		message_payload_t * payload = (message_payload_t *)(packet + sizeof(serial_header_t));
		NodeModel * current_node = GetModel(ntohs(payload->node_id));
		current_node->UpdateSensorData(ntohs(payload->sensor_data));
	}

	if(header->type == EXT_TYPE || header->type == EXT2_TYPE || header->type == EXT3_TYPE || header->type == EXT4_TYPE)
	{
		ext_message_payload_t * payload = (ext_message_payload_t *)(packet + sizeof(serial_header_t));
		NodeModel  * current_node = GetModel(ntohs(payload->node_id));
		ProcessInfo(current_node, (INFO_TYPES)payload->info_type, ntohs(payload->info_value));
	}

	if(header->type == EXT2_TYPE || header->type == EXT3_TYPE || header->type == EXT4_TYPE)
	{
		ext2_message_payload_t * payload = (ext2_message_payload_t *)(packet + sizeof(serial_header_t));
		NodeModel  * current_node = GetModel(ntohs(payload->node_id));
		ProcessInfo(current_node, (INFO_TYPES)payload->info_type2, ntohs(payload->info_value2));
	}

	if(header->type == EXT3_TYPE || header->type == EXT4_TYPE )
	{
		ext3_message_payload_t * payload = (ext3_message_payload_t *)(packet + sizeof(serial_header_t));
		NodeModel  * current_node = GetModel(ntohs(payload->node_id));
		ProcessInfo(current_node, (INFO_TYPES)payload->info_type3, ntohs(payload->info_value3));
	}

	if(header->type == EXT4_TYPE)
	{
		ext4_message_payload_t * payload = (ext4_message_payload_t *)(packet + sizeof(serial_header_t));
		NodeModel  * current_node = GetModel(ntohs(payload->node_id));
		ProcessInfo(current_node, (INFO_TYPES)payload->info_type4, ntohs(payload->info_value4));
	}

	if(header->type == INFO_ONLY)
	{
		info_payload_t * payload = (info_payload_t *)(packet + sizeof(serial_header_t));
		NodeModel  * current_node = GetModel(ntohs(payload->node_id));
		ProcessInfo(current_node, (INFO_TYPES)payload->info_type, ntohs(payload->info_value));
	}
}

static void ProcessInfo(NodeModel * current_node, INFO_TYPES info_type, uint16_t info_value)
{
	if(info_type == FOUND_NODE || info_type == LOST_NODE)
	{
		NodeModel * neighbor_node = GetModel(info_value);
		if(current_node->UpdateNeighborHeard(neighbor_node, info_type))
		{
			current_node->UpdateDb(db);
		}
		if(neighbor_node->UpdateNeighborHeardBy(current_node, info_type))
		{
			neighbor_node->UpdateDb(db);
		}
	}
	else if(info_type == PARENT_NODE)
	{
		NodeModel * parent_node = GetModel(info_value);
		if(current_node->UpdateParent(parent_node))
		{
			current_node->UpdateDb(db);
		}
	}
	else if(info_type == VOLTAGE_DATA)
	{
		if(current_node->UpdateBatteryData(info_value))
		{
			current_node->UpdateDb(db);
		}
	}
	else if(info_type == BOOTED)
	{
		current_node->UpdateBooted();
		current_node->UpdateDb(db);
	}
}

static NodeModel * GetModel(unsigned int node_id)
{
	//Create if it doesn't exist
	if(!node_models.count(node_id))
	{
		node_models[node_id] = NodeModel(node_id);
		node_models[node_id].UpdateDb(db);
	}

	return &node_models[node_id];
}

unsigned long long int GetTime()
{
	time_t seconds;
	seconds = time(NULL);
	return seconds;
}
