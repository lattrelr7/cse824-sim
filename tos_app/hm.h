#ifndef HMAPP_H
#define HMAPP_H

//#define SNOOP_MODE
//#define REAL_VOLTAGE

#define SENSE_PERIOD_MILLI 10000
#define ALIVE_PERIOD_MILLI (3 * SENSE_PERIOD_MILLI) //30000
#define REPEAT_INFO_MILLI (12 * ALIVE_PERIOD_MILLI) //240000
#define MAX_NEIGHBORS 25
#define UART_QUEUE_LEN 12
#define RADIO_QUEUE_LEN 12
#define INFO_QUEUE_LEN MAX_NEIGHBORS
#define BASE_STATION_NODE_ID 0
#define ALIVE_RESET_COUNT 3
#define ALIVE_DEC_BY 1

enum MESSAGE_TYPES
{
	SENSOR_TYPE = 0, /* Periodic message from sensor */
	EXT_TYPE = 4, /* Extended sensor type message */
	NETWORK_TYPE = 1, /* Command from CMC (routing) */
	FAULT_TYPE = 2, /* Fault injection message */
	ALIVE_TYPE = 3, /* Node alive broadcast - no payload */
	INFO_ONLY = 5, /* Message from sink */
};

enum FAULT_TYPES
{
	UNDEF = 0x01, /* */
	FAIL_ALIVE = 0x02, /* Stop alive tx */
	FAIL_DATA = 0x04, /* Stop sensor data tx */
	FAIL_BATTERY = 0x08, /* Stop both */
};

enum NETWORK_COMMAND_TYPES
{
	UPDATE_ROUTE,
};

enum INFO_TYPES
{
	FOUND_NODE = 0,
	LOST_NODE = 1,
	PARENT_NODE = 2,
	VOLTAGE_DATA = 3,
	BOOTED = 4,
};

typedef nx_struct neighbor_t {
	nx_uint16_t node_id;
	nx_uint8_t count;
	nx_uint8_t hops_to_sink;
	nx_uint8_t valid;
} neighbor_t;

typedef nx_struct info_t {
	nx_uint8_t info_type;
	nx_uint16_t info_value;
} info_t;

/* Alive message */
typedef nx_struct alive_message_t {
	nx_uint8_t hops_to_sink;
} alive_message_t;

/* Fault injection */
typedef nx_struct fault_command_t {
	nx_uint8_t fault_type;
} fault_command_t;

/* Broadcast and routing */
typedef nx_struct network_command_t {
	nx_uint8_t cmd_id;
	nx_uint8_t cmd_num;
} network_command_t;

typedef nx_struct network_route_t {
	nx_uint8_t cmd_id;
	nx_uint8_t hops_to_sink;
} network_route_t;

/* General payload structure */
typedef nx_struct message_payload_t {
	nx_uint8_t ttl;
	nx_uint16_t node_id;
	nx_uint16_t sensor_data;
} message_payload_t;

/* Extended payload structure */
typedef nx_struct ext_message_payload_t {
	nx_uint8_t ttl;
	nx_uint16_t node_id;
	nx_uint16_t sensor_data;
	nx_uint8_t info_type;
	nx_uint16_t info_value;
} ext_message_payload_t;

typedef nx_struct info_payload_t {
	nx_uint16_t node_id;
	nx_uint8_t info_type;
	nx_uint16_t info_value;
} info_payload_t;

#endif
