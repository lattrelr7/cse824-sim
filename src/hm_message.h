/*
 * message.h
 *
 *  Created on: Oct 7, 2015
 *      Author: ryan
 */

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

struct health_data_t {
	uint16_t p1;
} __attribute__ ((packed));

struct message_payload_t {
	uint16_t node_id;
	uint16_t sensor_reading;
	uint8_t sensor_status;
} __attribute__ ((packed));

struct network_command_t {
	uint8_t cmd_id;
	uint8_t cmd_num;
} __attribute__ ((packed));
