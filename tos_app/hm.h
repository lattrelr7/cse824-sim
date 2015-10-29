#ifndef HMAPP_H
#define HMAPP_H

enum 
{
	TIMER_PERIOD_MILLI = 5000,
	UART_QUEUE_LEN = 12,
	RADIO_QUEUE_LEN = 12,
	BASE_STATION_NODE_ID = 0,
};

enum MESSAGE_TYPES
{
	SENSOR_TYPE = 0, /* Periodic message from sensor */
	NETWORK_TYPE = 1, /* Command from CMC (routing) */
	FAULT_TYPE = 2, /* Fault injection message */
};

enum FAULT_TYPES
{
	BAD_SENSOR_READINGS = 0x01, /* Return abnormal values */
	FAIL_SENSOR = 0x02, /* Report BIT fault */
	FAIL_RADIO = 0x04, /* Intermittent responses */
	FAIL_BATTERY = 0x08, /* Stop responding */
};

enum BIT_TYPES
{
	BIT_OK = 0,
	BIT_FAILED = 1,
};

enum NETWORK_COMMAND_TYPES
{
	UPDATE_ROUTE,
};

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

/* HM specific data */
typedef nx_struct health_data_t {
	nx_uint16_t p1;
} health_data_t;

/* General payload structure */
typedef nx_struct message_payload_t {
	nx_uint16_t node_id;
	nx_uint16_t sensor_reading;
	nx_uint8_t sensor_status;
	nx_uint16_t next_hop;
} message_payload_t;

#endif
