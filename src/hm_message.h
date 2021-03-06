/*
 * message.h
 *
 *  Created on: Oct 7, 2015
 *      Author: ryan
 */

#ifndef CSE824_SRC_HMMESSAGE_H_
#define CSE824_SRC_HMMESSAGE_H_

#define BASE_STATION_ID 0

enum MESSAGE_TYPES
{
	SENSOR_TYPE = 0, /* Periodic message from sensor */
	EXT_TYPE = 4, /* Extended sensor type message */
	NETWORK_TYPE = 1, /* Command from CMC (routing) */
	FAULT_TYPE = 2, /* Fault injection message */
	ALIVE_TYPE = 3, /* Node alive broadcast - no payload */
	INFO_ONLY = 5, /* Message from sink */
	EXT2_TYPE = 6,
	EXT3_TYPE = 7,
	EXT4_TYPE = 8,
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
	uint8_t ttl;
	uint16_t node_id;
	uint16_t sensor_data;
} __attribute__ ((packed));

struct ext_message_payload_t {
	uint8_t ttl;
	uint16_t node_id;
	uint16_t sensor_data;
	uint8_t info_type;
	uint16_t info_value;
} __attribute__ ((packed));

struct ext2_message_payload_t {
	uint8_t ttl;
	uint16_t node_id;
	uint16_t sensor_data;
	uint8_t info_type;
	uint16_t info_value;
	uint8_t info_type2;
	uint16_t info_value2;
} __attribute__ ((packed));

struct ext3_message_payload_t {
	uint8_t ttl;
	uint16_t node_id;
	uint16_t sensor_data;
	uint8_t info_type;
	uint16_t info_value;
	uint8_t info_type2;
	uint16_t info_value2;
	uint8_t info_type3;
	uint16_t info_value3;
} __attribute__ ((packed));

struct ext4_message_payload_t {
	uint8_t ttl;
	uint16_t node_id;
	uint16_t sensor_data;
	uint8_t info_type;
	uint16_t info_value;
	uint8_t info_type2;
	uint16_t info_value2;
	uint8_t info_type3;
	uint16_t info_value3;
	uint8_t info_type4;
	uint16_t info_value4;
} __attribute__ ((packed));

struct info_payload_t {
	uint16_t node_id;
	uint8_t info_type;
	uint16_t info_value;
} __attribute__ ((packed));

struct network_command_t {
	uint8_t cmd_id;
	uint8_t cmd_num;
} __attribute__ ((packed));

#endif /* CSE824_SRC_HMMESSAGE_H_ */
