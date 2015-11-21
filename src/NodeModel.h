/*
 * NodeModel.h
 *
 *  Created on: Nov 13, 2015
 *      Author: ryan
 */

#ifndef CSE824_SRC_NODEMODEL_H_
#define CSE824_SRC_NODEMODEL_H_

#include <list>
#include <string>
#include <sqlite3.h>
#include "hm_message.h"

#define ROUTE_STABLE_TIME 60
#define MISSED_MESSAGE_TIME 60
#define CLEAR_FAULT(x,y) (x = x & ~y)

class NodeModel {
public:
	enum NODE_STATE{
		GOOD = 0x0000, /* None of the below */
		DATA_FAILURE = 0x0001, /* Not receiving data after certain time out */
		PATH_FAILURE = 0x0002, /* Path broken due to parent node failure */
		PATH_UPDATE = 0x0004, /* If parent has changed recently */
		IN_LINK_ERRORS_MINOR = 0x0008, /* If num heard or num heard_by >= 75% of max */
		IN_LINK_ERRORS_MAJOR = 0x0010, /* If num heard or num heard_by < 75% of max */
		IN_LINK_FAILURE = 0x0020, /* If heard or heard_by is 0 */
		OUT_LINK_ERRORS_MINOR = 0x0040, /* If num heard or num heard_by >= 75% of max */
		OUT_LINK_ERRORS_MAJOR = 0x0080, /* If num heard or num heard_by < 75% of max */
		OUT_LINK_FAILURE = 0x0100, /* If heard or heard_by is 0 */
		BATTERY_FAILURE = 0x0200, /* Battery problems detected */
	};

	NodeModel();
	NodeModel(unsigned int id);
	virtual ~NodeModel();
	bool UpdateNodeState();
	bool UpdateBatteryData(unsigned int voltage);
	void UpdateSensorData(unsigned int data);
	bool UpdateNeighborHeardBy(NodeModel * node, INFO_TYPES info_type);
	bool UpdateNeighborHeard(NodeModel * node, INFO_TYPES info_type);
	bool UpdateParent(NodeModel * node);
	void UpdateDb(sqlite3 * db);
	std::string PrintNode();
	std::string PrintSummary();
	std::string PrintTopology(int level);
	unsigned int GetId() {return _id;}

private:
	std::string ListToString(std::list<NodeModel*> node_list);
	bool IsRouteBroken();
	bool AreChildrenBroken();
	bool AreSelfAndChildrenBroken();
	bool IsBatteryHealthy();
	unsigned long long int GetTime();
	bool IsNodeInList(NodeModel * node, std::list<NodeModel *> list);

	NodeModel * _parent;
	std::list<NodeModel *> _children;
	std::list<NodeModel *> _heard_by;
	std::list<NodeModel *> _heard;
	std::list<NodeModel *> _n_heard_by;
	std::list<NodeModel *> _n_heard;

	bool _is_sink;
	unsigned int _id;
	unsigned int _parent_id;
	unsigned int _path_failed_id;
	unsigned int _msgs_rxed;
	unsigned int _max_heard;
	unsigned int _max_heard_by;
	unsigned long long int _last_msg_timestamp;
	unsigned long long int _last_route_update_timestamp;
	unsigned long long int _avg_message_arrival_rate;
	unsigned int _last_sensor_value;
	unsigned int _last_voltage_value;
	unsigned long long int _avg_sensor_value;
	unsigned long long int _sensor_value_count;
	unsigned long long int _avg_voltage_diff;
	unsigned long long int _voltage_value_count;
	unsigned long long int _state_duration;
	unsigned long long int _last_state_change_timestamp;
	unsigned int _current_state;
};


#endif /* CSE824_SRC_NODEMODEL_H_ */
