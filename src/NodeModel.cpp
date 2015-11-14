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
}

NodeModel::~NodeModel() {
	// TODO Auto-generated destructor stub
}

void NodeModel::UpdateNodeState()
{
	unsigned int start_state = _current_state;

	/* Not receiving data after certain time out */
	if(GetTime() -_last_msg_timestamp > MISSED_MESSAGE_TIME)
	{
		_current_state |= DATA_FAILURE;
	}
	else
	{
		CLEAR_FAULT(_current_state, DATA_FAILURE);
	}

	if(!IsBatteryHealthy())
	{
		_current_state |= BATTERY_FAILURE;
	}
	else
	{
		CLEAR_FAULT(_current_state, BATTERY_FAILURE);
	}

	/* Path broken due to parent node failure */
	if(IsRouteBroken())
	{
		_current_state |= PATH_FAILURE;
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

	ss << PrintSummary();

	ss << "Node Status:" << std::endl;
	if(failures.size() != 0)
	{
		for(unsigned int i=0; i < failures.size(); i++)
		{
			ss << failures[i] << std::endl;
		}
	}
	else
	{
		ss << "GOOD" << std::endl;
	}

	ss << std::endl;

	return ss.str();
}

void NodeModel::UpdateBatteryData(unsigned int voltage)
{

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

void NodeModel::UpdateNeighborHeardBy(NodeModel * node, INFO_TYPES info_type)
{
	if(info_type == FOUND_NODE)
	{
		//If node is in neither list add to max
		if(!IsNodeInList(node, _heard_by) && !IsNodeInList(node, _n_heard_by))
		{
			_max_heard_by++;
		}

		//Check if node is already heard
		if(!IsNodeInList(node, _heard_by))
		{
			_heard_by.push_back(node);
		}

		//Check if node is in _n_heard_by
		if(IsNodeInList(node, _n_heard_by))
		{
			_n_heard_by.remove(node);
		}
	}
	else
	{
		_heard_by.remove(node);
		_n_heard_by.push_back(node);
	}
}

void NodeModel::UpdateNeighborHeard(NodeModel * node, INFO_TYPES info_type)
{
	if(info_type == FOUND_NODE)
	{
		//If node is in neither list add to max
		if(!IsNodeInList(node, _heard) && !IsNodeInList(node, _n_heard))
		{
			_max_heard++;
		}

		//Check if node is already heard
		if(!IsNodeInList(node, _heard))
		{
			_heard.push_back(node);
		}

		//Check if node is in _n_heard
		if(IsNodeInList(node, _n_heard))
		{
			_n_heard.remove(node);
		}
	}
	else
	{
		_heard.remove(node);
		_n_heard.push_back(node);
	}
}

void NodeModel::UpdateParent(NodeModel * node)
{
	if(_parent != node)
	{
		if(_parent != NULL)
		{
			_current_state |= PATH_UPDATE;
			_last_route_update_timestamp = GetTime();
			_parent->_children.remove(this);
		}

		_parent = node;
		_parent_id = node->_id;
		_parent->_children.push_back(this);
	}
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
	return route_broken;
}

bool NodeModel::IsBatteryHealthy()
{
	bool battery_healthy = true;
	return battery_healthy;
}
