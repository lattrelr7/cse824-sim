/*
 * message.h
 *
 *  Created on: Oct 7, 2015
 *      Author: ryan
 */

enum MESSAGE_TYPES
{
	SENSOR_TYPE = 0, /* Periodic message from sensor */
	EXT_TYPE = 4, /* Extended sensor type message */
	NETWORK_TYPE = 1, /* Command from CMC (routing) */
	FAULT_TYPE = 2, /* Fault injection message */
	ALIVE_TYPE = 3, /* Node alive broadcast - no payload */
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
	FOUND_NODE,
	LOST_NODE,
};

/*
 * Any fields that are not 8 bits need to be converted
 * to/from big endian
 */

struct fault_command_t {
	uint8_t fault_type;
} __attribute__ ((packed));

struct serial_header_t {
	uint16_t dest;
	uint16_t src;
	uint8_t length;
	uint8_t group;
	uint8_t type;
} __attribute__ ((packed));

struct message_payload_t {
	uint16_t node_id;
	uint16_t voltage;
	uint16_t sensor_data;
} __attribute__ ((packed));

struct ext_message_payload_t {
	uint16_t node_id;
	uint16_t voltage;
	uint16_t sensor_data;
	uint8_t info_type;
	uint16_t info_addr;
} __attribute__ ((packed));

struct network_command_t {
	uint8_t cmd_id;
	uint8_t cmd_num;
} __attribute__ ((packed));
