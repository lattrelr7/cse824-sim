#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sf/serialpacket.h>
#include <sf/serialsource.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#include <time.h>
#include <sqlite3.h>
#include <iostream>
#include <string>
#include <map>
#include <hm_message.h>
#include <synchronized_queue.h>
#include <NodeModel.h>

#define TIME_LIMIT 0

static void hexprint(uint8_t *packet, int len);
static void * InputHandlerThread(void * args);
static void * SerialListenThread(void * args);
static void UpdateDbFaultData(int nodeId, int value);
static void * UpdateAndLogNodeModel(void * args);

static void LogTcMsg( const char * format, ... );
static void PrintOptions();
static void UpdateFaultModel(uint8_t * packet);
static void ProcessInfo(NodeModel * node, INFO_TYPES info_type, uint16_t info_value);
static NodeModel * GetModel(unsigned int node_id);
static unsigned long long int GetTime();
static SynchronizedQueue<message_payload_t> my_queue;
static sqlite3 *db;
static std::map<unsigned int, NodeModel> node_models;
static long long unsigned int data_rxed = 0;
static long long unsigned int hm_rxed = 0;
static long long unsigned int start_time = 0;

static char *msgs[] = {
  "unknown_packet_type",
  "ack_timeout"	,
  "sync"	,
  "too_long"	,
  "too_short"	,
  "bad_sync"	,
  "bad_crc"	,
  "closed"	,
  "no_memory"	,
  "unix_error"
};

int main()
{
	int rc = sqlite3_open("824Sim.db", &db);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

	pthread_t input_thread, serial_rx_thread, model_thread;


	pthread_create(&input_thread, NULL, InputHandlerThread, NULL);
	pthread_create(&serial_rx_thread, NULL, SerialListenThread, NULL);
	pthread_create(&model_thread, NULL, UpdateAndLogNodeModel, NULL);
	

	std::cout << "Starting event loop..." << std::endl;

	start_time = GetTime();
	while(1)
	{
		usleep(1000000);
	}
}

void stderr_msg(serial_source_msg problem)
{
  fprintf(stderr, "Note: %s\n", msgs[problem]);
}

static void * InputHandlerThread(void * args)
{
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
			std::cout << "*****Feature deprecated for real hardware******" << std::endl;
			//SendNetworkCommand(t, UPDATE_ROUTE);
		}
		else if(sscanf(input_command, "fault %d %s", &i_arg1, s_arg1) > 0)
		{
			std::cout << "*****Feature deprecated for real hardware******" << std::endl;
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
	LogTcMsg("len = %d\n", len);
	for (int i = 0; i < len; i++)
	{
		printf("%02x ", packet[i] & 0xff);
	}
	printf("\n");
}

static void * SerialListenThread(void * args)
{
	serial_source src;
	
	src = open_serial_source("/dev/ttyS5", 57600, 0, stderr_msg);
	if (!src)
    {
      perror("Couldn't open serial port");
      exit(1);
    }
	
	while(1)
	{
		usleep(1000);
		int len;
			
		const unsigned char *packet = read_serial_packet(src, &len);
		
		if (!packet)
		{
			printf("No Packet Read");
			exit(0);
		}
		else
		{
			// We get an extra "termination byte" from the serial read. Therefore we move forward 1 in memory to account for it.
			packet += 1;
			UpdateFaultModel(packet);

			serial_header_t * header = (serial_header_t *)packet;

			if(header->type == SENSOR_TYPE || header->type == EXT_TYPE)
			{
				message_payload_t * payload = (message_payload_t *)(packet + sizeof(serial_header_t));
				//hexprint((uint8_t*)packet, len);
				LogTcMsg("*********NEW BASE MESSAGE*********\n");
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
