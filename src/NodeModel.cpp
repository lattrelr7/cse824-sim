/*
 * NodeModel.cpp
 *
 *  Created on: Nov 13, 2015
 *      Author: ryan
 */

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <time.h>
#include <vector>
#include <string>
#include "NodeModel.h"

NodeModel::NodeModel()
{
	_is_sink = true;
	_parent = NULL;
	_id = 0;
	_parent_id = 0;
	_msgs_rxed = 0;
	_last_msg_timestamp = 0;
	_last_route_update_timestamp = 0;
	_avg_message_arrival_rate = 0;
	_last_sensor_value = 0;
	_last_voltage_value = 0;
	_avg_sensor_value = 0;
	_avg_voltage_diff = 0;
	_state_duration = 0;
	_last_state_change_timestamp = GetTime();
	_current_state = GOOD;
	_max_heard = 0;
	_max_heard_by = 0;
	_sensor_value_count = 0;
	_avg_voltage_diff = 0;
	_voltage_value_count = 0;
	_path_failed_id = 0;
}

NodeModel::NodeModel(unsigned int id)
{
	if(id == BASE_STATION_ID)
	{
		_is_sink = true;
	}
	else
	{
		_is_sink = false;
	}
	_parent = NULL;
	_id = id;
	_parent_id = 0;
	_msgs_rxed = 0;
	_last_msg_timestamp = 0;
	_last_route_update_timestamp = 0;
	_avg_message_arrival_rate = 0;
	_last_sensor_value = 0;
	_last_voltage_value = 0;
	_avg_sensor_value = 0;
	_avg_voltage_diff = 0;
	_state_duration = 0;
	_last_state_change_timestamp = GetTime();
	_current_state = GOOD;
	_max_heard = 0;
	_max_heard_by = 0;
	_sensor_value_count = 0;
	_avg_voltage_diff = 0;
	_voltage_value_count = 0;
	_path_failed_id = 0;
}

NodeModel::~NodeModel() {
	// TODO Auto-generated destructor stub
}

bool NodeModel::UpdateNodeState()
{
	unsigned int start_state = _current_state;

	/* Not receiving data after certain time out */
	if(!_is_sink)
	{
		if(GetTime() -_last_msg_timestamp > MISSED_MESSAGE_TIME)
		{
			_current_state |= DATA_FAILURE;
		}
		else
		{
			CLEAR_FAULT(_current_state, DATA_FAILURE);
		}
	}

	if(!IsBatteryHealthy())
	{
		_current_state |= BATTERY_FAILURE;
	}
	else
	{
		CLEAR_FAULT(_current_state, BATTERY_FAILURE);
	}

	/* Path broken due to parent node failure or all children
	 * being reported as failing */
	if(IsRouteBroken())
	{
		_current_state |= PATH_FAILURE;
	}
	else if(AreChildrenBroken())
	{
		_current_state |= PATH_FAILURE;
		_path_failed_id = _id;
	}
	else
	{
		CLEAR_FAULT(_current_state, PATH_FAILURE);
	}

	/* If parent has changed recently */
	if(GetTime() - _last_route_update_timestamp > ROUTE_STABLE_TIME)
	{
		CLEAR_FAULT(_current_state, PATH_UPDATE);
	}

	/* Handle heard erros, links coming IN to the node */
	float heard_perc = float(_heard.size())/float(_max_heard);
	if(heard_perc == 1.00)
	{
		CLEAR_FAULT(_current_state, IN_LINK_FAILURE);
		CLEAR_FAULT(_current_state, IN_LINK_ERRORS_MAJOR);
		CLEAR_FAULT(_current_state, IN_LINK_ERRORS_MINOR);
	}
	else if(heard_perc >= 0.75 && heard_perc < 1.00)
	{
		CLEAR_FAULT(_current_state, IN_LINK_FAILURE);
		CLEAR_FAULT(_current_state, IN_LINK_ERRORS_MAJOR);
		_current_state |= IN_LINK_ERRORS_MINOR;
	}
	else if(heard_perc > 0 && heard_perc < 0.75)
	{
		CLEAR_FAULT(_current_state, IN_LINK_FAILURE);
		CLEAR_FAULT(_current_state, IN_LINK_ERRORS_MINOR);
		_current_state |= IN_LINK_ERRORS_MAJOR;
	}
	else if(heard_perc == 0)
	{
		CLEAR_FAULT(_current_state, IN_LINK_ERRORS_MAJOR);
		CLEAR_FAULT(_current_state, IN_LINK_ERRORS_MINOR);
		_current_state |= IN_LINK_FAILURE;
	}

	/* Handle heard_by errors, links coming OUT of the node */
	float heard_by_perc = float(_heard_by.size())/float(_max_heard_by);
	if(heard_by_perc == 1.00)
	{
		CLEAR_FAULT(_current_state, OUT_LINK_FAILURE);
		CLEAR_FAULT(_current_state, OUT_LINK_ERRORS_MAJOR);
		CLEAR_FAULT(_current_state, OUT_LINK_ERRORS_MINOR);
	}
	else if(heard_by_perc >= 0.75 && heard_by_perc < 1.00)
	{
		CLEAR_FAULT(_current_state, OUT_LINK_FAILURE);
		CLEAR_FAULT(_current_state, OUT_LINK_ERRORS_MAJOR);
		_current_state |= OUT_LINK_ERRORS_MINOR;
	}
	else if(heard_by_perc > 0 && heard_by_perc < 0.75)
	{
		CLEAR_FAULT(_current_state, OUT_LINK_FAILURE);
		CLEAR_FAULT(_current_state, OUT_LINK_ERRORS_MINOR);
		_current_state |= OUT_LINK_ERRORS_MAJOR;
	}
	else if(heard_by_perc == 0)
	{
		CLEAR_FAULT(_current_state, OUT_LINK_ERRORS_MAJOR);
		CLEAR_FAULT(_current_state, OUT_LINK_ERRORS_MINOR);
		_current_state |= OUT_LINK_FAILURE;
	}

	if(start_state != _current_state)
	{
		_last_state_change_timestamp = GetTime();
	}

	PrintNode();

	return (_current_state != start_state);
}

std::string NodeModel::PrintSummary()
{
	std::ostringstream ss;
	ss << "[" << _id << "]" << std::endl;
	ss << "Heard:" << _heard.size() << "/" << _max_heard << std::endl;
	ss << "Heard by:" << _heard_by.size() << "/" << _max_heard_by << std::endl;
	ss << "Avg msg rate: " << _avg_message_arrival_rate << std::endl;
	ss << "Avg sensor value: " << _avg_sensor_value << std::endl;
	return ss.str();
}

std::string NodeModel::PrintNode()
{
	std::ostringstream ss;
	std::vector<std::string> failures;
	if(_current_state & DATA_FAILURE) {failures.push_back("DATA_FAILURE");}
	if(_current_state & PATH_FAILURE) {failures.push_back("PATH_FAILURE");}
	if(_current_state & PATH_UPDATE) {failures.push_back("PATH_UPDATE");}
	if(_current_state & IN_LINK_ERRORS_MINOR) {failures.push_back("IN_LINK_ERRORS_MINOR");}
	if(_current_state & IN_LINK_ERRORS_MAJOR) {failures.push_back("IN_LINK_ERRORS_MAJOR");}
	if(_current_state & IN_LINK_FAILURE) {failures.push_back("IN_LINK_FAILURE");}
	if(_current_state & OUT_LINK_ERRORS_MINOR) {failures.push_back("OUT_LINK_ERRORS_MINOR");}
	if(_current_state & OUT_LINK_ERRORS_MAJOR) {failures.push_back("OUT_LINK_ERRORS_MAJOR");}
	if(_current_state & OUT_LINK_FAILURE) {failures.push_back("OUT_LINK_FAILURE");}
	if(_current_state & BATTERY_FAILURE) {failures.push_back("BATTERY_FAILURE");}

	if(_current_state == 0x000)
	{
		return "";
	}

	ss << PrintSummary();

	ss << "Node Status:" << std::endl;
	if(failures.size() != 0)
	{
		for(unsigned int i=0; i < failures.size(); i++)
		{
			ss << failures[i];
			if(failures[i] == "PATH_FAILURE") {ss << " AT " << _path_failed_id;}
			if(failures[i] == "PATH_UPDATE") {ss << " TO " << _parent_id;}
			if(failures[i] == "OUT_LINK_ERRORS_MINOR" || failures[i] == "OUT_LINK_ERRORS_MAJOR")
			{
				ss << "\n" << "\tCAN'T BE HEARD BY ";
				for (std::list<NodeModel *>::iterator it=_n_heard_by.begin(); it!=_n_heard_by.end(); ++it)
				{
					ss << (*it)->_id << " ";
				}
			}
			if(failures[i] == "IN_LINK_ERRORS_MINOR" || failures[i] == "IN_LINK_ERRORS_MAJOR")
			{
				ss << "\n" << "\tCAN'T HEAR ";
				for (std::list<NodeModel *>::iterator it=_n_heard.begin(); it!=_n_heard.end(); ++it)
				{
					ss << (*it)->_id << " ";
				}
			}

			ss<< std::endl;
		}
	}
	else
	{
		ss << "GOOD" << std::endl;
	}

	ss << std::endl;

	return ss.str();
}

bool NodeModel::UpdateBatteryData(unsigned int voltage)
{
	bool changed = false;
	if(_last_voltage_value != voltage)
	{
		_last_voltage_value = voltage;
		changed = true;
	}

	return changed;
}

void NodeModel::UpdateSensorData(unsigned int data)
{
	_last_sensor_value = data;
	_sensor_value_count++;
	if(_sensor_value_count == 1)
	{
		_avg_sensor_value = data;
	}
	_avg_sensor_value = float((((_sensor_value_count - 1) * _avg_sensor_value) + data))/float(_sensor_value_count);

	if(_sensor_value_count > 2)
	{
		unsigned long long int current_diff = GetTime() - _last_msg_timestamp;
		_avg_message_arrival_rate = float((((_sensor_value_count - 1) * _avg_message_arrival_rate) + current_diff))/float(_sensor_value_count);
	}
	else if(_sensor_value_count == 2)
	{
		_avg_message_arrival_rate = GetTime() - _last_msg_timestamp;
	}
	_last_msg_timestamp = GetTime();
}

bool NodeModel::UpdateNeighborHeardBy(NodeModel * node, INFO_TYPES info_type)
{
	bool changed = false;
	if(info_type == FOUND_NODE)
	{
		//If node is in neither list add to max
		if(!IsNodeInList(node, _heard_by) && !IsNodeInList(node, _n_heard_by))
		{
			_max_heard_by++;
			changed = true;
		}

		//Check if node is already heard
		if(!IsNodeInList(node, _heard_by))
		{
			_heard_by.push_back(node);
			changed = true;
		}

		//Check if node is in _n_heard_by
		if(IsNodeInList(node, _n_heard_by))
		{
			_n_heard_by.remove(node);
			changed = true;
		}
	}
	else
	{
		_heard_by.remove(node);

		if(!IsNodeInList(node, _n_heard_by))
		{
			_n_heard_by.push_back(node);
			changed = true;
		}
	}
	return changed;
}

bool NodeModel::UpdateNeighborHeard(NodeModel * node, INFO_TYPES info_type)
{
	bool changed = false;
	if(info_type == FOUND_NODE)
	{
		//If node is in neither list add to max
		if(!IsNodeInList(node, _heard) && !IsNodeInList(node, _n_heard))
		{
			_max_heard++;
			changed = true;
		}

		//Check if node is already heard
		if(!IsNodeInList(node, _heard))
		{
			_heard.push_back(node);
			changed = true;
		}

		//Check if node is in _n_heard
		if(IsNodeInList(node, _n_heard))
		{
			_n_heard.remove(node);
			changed = true;
		}
	}
	else
	{
		_heard.remove(node);

		if(!IsNodeInList(node, _n_heard))
		{
			_n_heard.push_back(node);
			changed = true;
		}
	}
	return changed;
}

bool NodeModel::UpdateParent(NodeModel * node)
{
	bool changed = false;
	if(_parent != node)
	{
		changed = true;
		if(_parent != NULL)
		{

			_parent->_children.remove(this);
		}

		_current_state |= PATH_UPDATE;
		_last_route_update_timestamp = GetTime();
		_parent = node;
		_parent_id = node->_id;
		_parent->_children.push_back(this);
	}
	return changed;
}

unsigned long long int NodeModel::GetTime()
{
	time_t seconds;
	seconds = time(NULL);
	return seconds;
}

bool NodeModel::IsNodeInList(NodeModel * node, std::list<NodeModel *> list)
{
	bool found = false;
	for (std::list<NodeModel *>::iterator it=list.begin(); it!=list.end(); ++it)
	{
		if(node == *it)
		{
			found = true;
			break;
		}
	}
	return found;
}

bool NodeModel::IsRouteBroken()
{
	bool route_broken = false;
	std::vector<NodeModel*> node_hops;
	NodeModel * next_hop = _parent;

	while(next_hop != NULL)
	{
		node_hops.push_back(next_hop);
		next_hop = next_hop->_parent;
	}

	/* Go backwards and find the failure closest to the root */
	for(int i=node_hops.size()-1; i >= 0; i--)
	{
		if(node_hops[i]->_current_state & (IN_LINK_FAILURE|OUT_LINK_FAILURE|BATTERY_FAILURE|DATA_FAILURE|PATH_FAILURE))
		{
			route_broken = true;
			_path_failed_id = node_hops[i]->_id;
			break;
		}
	}

	return route_broken;
}

bool NodeModel::IsBatteryHealthy()
{
	bool battery_healthy = true;
	return battery_healthy;
}

bool NodeModel::AreChildrenBroken()
{
	bool children_failed = false;

	/* Only run this if there is more than 1 child, otherwise it is hard to tell
	 * who is to  blame for the failure
	 */
	if(_children.size() > 1)
	{
		children_failed = true;
		for (std::list<NodeModel *>::iterator it=_children.begin(); it!=_children.end(); ++it)
		{
			if((*it)->AreSelfAndChildrenBroken() == false)
			{
				children_failed = false;
				break;
			}
		}
	}

	return children_failed;
}

bool NodeModel::AreSelfAndChildrenBroken()
{
	bool children_failed = false;

	if(_children.size() != 0)
	{
		children_failed = true;
		for (std::list<NodeModel *>::iterator it=_children.begin(); it!=_children.end(); ++it)
		{
			if((*it)->AreSelfAndChildrenBroken() == false)
			{
				children_failed = false;
				break;
			}
		}
	}

	if(_current_state & (IN_LINK_FAILURE|OUT_LINK_FAILURE|BATTERY_FAILURE|DATA_FAILURE))
	{
		children_failed = true;
	}

	return children_failed;
}

/* Write out tree from this node */
std::string NodeModel::PrintTopology(int level)
{
	std::ostringstream ss;

	for(int i=0; i < level; i++) { ss << "\t"; }
	ss << "NODE-" << _id << std::endl;

	for (std::list<NodeModel *>::iterator it=_children.begin(); it!=_children.end(); ++it)
	{
		ss << (*it)->PrintTopology(level + 1);
	}

	return ss.str();
}

std::string NodeModel::ListToString(std::list<NodeModel*> node_list)
{
	std::ostringstream ss;
	unsigned int count = 0;
	ss << "'";
	for (std::list<NodeModel *>::iterator it=node_list.begin(); it!=node_list.end(); ++it)
	{
		ss << (*it)->GetId();
		if(count != node_list.size() - 1)
		{
			ss << ",";
		}
		count++;
	}
	ss << "'";
	return ss.str();
}

void NodeModel::UpdateDb(sqlite3 * db)
{
	const char insertTemplate[] = "INSERT INTO NodeModel ("
			"CurrentTime,"
			"NodeId,"
			"ParentId,"
			"IsSink,"
			"CurrentState,"
			"PathFailedId,"
			"Children,"
			"NodesHeard,"
			"NodesNoLongerHeard,"
			"NodesHeardBy,"
			"NodesNoLongerHeardBy,"
			"MaxHeard,"
			"MaxHeardBy,"
			"MsgsRxed,"
			"LastSensorValue,"
			"LastVoltageValue,"
			"LastMsgTime,"
			"LastRouteUpdateTime,"
			"LastStateChangeTime"
			") VALUES(%llu,%d,%d,%d,%d,%d,%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%llu,%llu,%llu);";

	int rc = 0;
	char *err_msg = 0;
	char query[1024];

	sprintf(query, insertTemplate,
			GetTime(),
			_id,
			_parent_id,
			(int)_is_sink,
			_current_state,
			_path_failed_id,
			ListToString(_children).c_str(),
			ListToString(_heard).c_str(),
			ListToString(_n_heard).c_str(),
			ListToString(_heard_by).c_str(),
			ListToString(_n_heard_by).c_str(),
			_max_heard,
			_max_heard_by,
			_msgs_rxed,
			_last_sensor_value,
			_last_voltage_value,
			_last_msg_timestamp,
			_last_route_update_timestamp,
			_last_state_change_timestamp
			);

	rc = sqlite3_exec(db, query, 0, 0, &err_msg);

	if (rc != SQLITE_OK )
	{
		printf("SQL error: %s\nUsing SQL Query %s\n", err_msg, query);
		sqlite3_free(err_msg);
	}
}
