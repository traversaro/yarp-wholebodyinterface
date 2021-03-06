/*
 * Copyright (C)2011  Department of Robotics Brain and Cognitive Sciences - Istituto Italiano di Tecnologia
 * Author: Andrea Del Prete, Marco Randazzo
 * email: andrea.delprete@iit.it marco.randazzo@iit.it
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
 */

#include <iostream>
#include <algorithm>

#include "yarpWbiUtil.h"

#include <yarp/os/Log.h>
#include <yarp/os/LogStream.h>
#include <kdl_codyco/treeserialization.hpp>
#include <cmath>

static const std::string WBI_YARP_JOINTS_GROUP = "WBI_YARP_JOINTS";
const std::string yarpWbi::ErrorDomain = "wbi.yarp.error";


namespace yarpWbi
{
    const double Rad2Deg = 180.0 / M_PI;
    const double Deg2Rad = M_PI / 180.0;


bool openPolyDriver(const std::string &localName,
                    const std::string &robotName,
                    yarp::dev::PolyDriver *&pd,
                    const std::string &bodyPartName)
{
    std::string localPort  = "/" + localName + "/" + bodyPartName;
    std::string remotePort = "/" + robotName + "/" + bodyPartName;
    yarp::os::Property options;
    options.put("robot",robotName.c_str());
    options.put("part",bodyPartName.c_str());
    options.put("device","remote_controlboard");
    options.put("local",localPort.c_str());
    options.put("remote",remotePort.c_str());
    options.put("writeStrict","on");

    pd = new yarp::dev::PolyDriver(options);
    if(!pd || !(pd->isValid()))
    {
        yError("Problems instantiating the device driver %s", bodyPartName.c_str());
        return false;
    }
    return true;
}

bool closePolyDriver(yarp::dev::PolyDriver *&pd)
{
    if( !pd || !(pd->isValid()) )
    {
        return false;
    }
    bool ret = pd->close();

    delete pd;

    pd = 0;

    return ret;
}

yarp::os::Bottle & getWBIYarpJointsOptions(yarp::os::Property & wbi_yarp_properties)
{
    return wbi_yarp_properties.findGroup(WBI_YARP_JOINTS_GROUP);
}

bool appendNewControlBoardsToVector(yarp::os::Bottle & joints_config,
                                    const wbi::IDList & jointIdList,
                                    std::vector<std::string> & controlBoardNames)
{
    for(int jnt=0; jnt < (int)jointIdList.size(); jnt++ )
    {
        wbi::ID jnt_name;
        jointIdList.indexToID(jnt,jnt_name);

        if( !joints_config.check(jnt_name.toString().c_str()) )
        {
            yError() << "wholeBodyInterface error: joint " << jnt_name.toString() <<
                         "not found in WBI_YARP_JOINTS section of configuration file ";
            return false;
        }

        yarp::os::Bottle * ctrlBoard_mapping = joints_config.find(jnt_name.toString().c_str()).asList();
        if( !ctrlBoard_mapping || ctrlBoard_mapping->size() != 2 )
        {
            yError() << "wholeBodyInterface error: joint " << jnt_name.toString() <<
                         " found in WBI_YARP_JOINTS but is not in the canonical form (controlBoardName,axis)";
            return false;
        }

        std::string controlboard_name = ctrlBoard_mapping->get(0).asString().c_str();
        if (std::find(controlBoardNames.begin(), controlBoardNames.end(), controlboard_name) == controlBoardNames.end())
        {
            controlBoardNames.push_back(controlboard_name);
        }
    }
    return true;
}

bool getControlBoardAxisList(yarp::os::Bottle & joints_config,
                                                          const wbi::IDList &jointIdList,
                                                          const std::vector<std::string>& controlBoardNames,
                                                          std::vector< std::pair<int,int> > & controlBoardAxisList)
{
    std::map<std::string,int> controlBoardIds = getControlBoardIdsMap(controlBoardNames);

    assert(controlBoardIds.size() == controlBoardNames.size());

    //Error check loop
    for(int wbi_jnt=0; wbi_jnt < (int)jointIdList.size(); wbi_jnt++ )
    {
        wbi::ID wbi_jnt_name;
        jointIdList.indexToID(wbi_jnt,wbi_jnt_name);

        //std::cout << joints_config.toString() << std::endl;

        if( joints_config.find(wbi_jnt_name.toString().c_str()).isNull() )
        {
            yError() << "yarpWbiUtil error: joint " << wbi_jnt_name.toString() << " not found in WBI_YARP_JOINTS section ";
            return false;
        }

        yarp::os::Bottle * ctrlBoard_mapping = joints_config.find(wbi_jnt_name.toString().c_str()).asList();

        if( ctrlBoard_mapping->size() != 2 )
        {
            yError() << "yarpWbiUtil error: joint " << wbi_jnt_name.toString() << " found in WBI_YARP_JOINTS section, but with wrong format ";
            return false;
        }

        std::string controlboard_name = ctrlBoard_mapping->get(0).asString().c_str();
        if( controlBoardIds.find(controlboard_name) == controlBoardIds.end() )
        {
            yError() << "yarpWbiUtil :: getControlBoardAxisList error in configuration files with joint " << wbi_jnt_name.toString();
            return false;
        }
    }

    controlBoardAxisList.resize(jointIdList.size());
    for(int wbi_jnt=0; wbi_jnt < (int)jointIdList.size(); wbi_jnt++ )
    {
        wbi::ID wbi_jnt_name;
        jointIdList.indexToID(wbi_jnt,wbi_jnt_name);

        yarp::os::Bottle * ctrlBoard_mapping = joints_config.find(wbi_jnt_name.toString().c_str()).asList();

        std::string controlboard_name = ctrlBoard_mapping->get(0).asString().c_str();


        int controlboard_jnt_axis = ctrlBoard_mapping->get(1).asInt();
        int controlboard_wbi_id   = controlBoardIds[controlboard_name];

        //totalControlledJointsInControlBoard[controlboard_wbi_id]++;
        controlBoardAxisList[wbi_jnt] = std::pair<int,int>(controlboard_wbi_id,controlboard_jnt_axis);
    }
    return true;
}

std::vector< int > getControlBoardList(const std::vector< std::pair<int,int> > & controlBoardAxisList)
{
    std::vector< int > controlBoardList;
    for(int jnt=0; jnt < (int)controlBoardAxisList.size(); jnt++ )
    {
        int wbi_ctrlBoard_numericId = controlBoardAxisList[jnt].first;
        if( std::find(controlBoardList.begin(), controlBoardList.end(), wbi_ctrlBoard_numericId) == controlBoardList.end() )
        {
            controlBoardList.push_back(wbi_ctrlBoard_numericId);
        }
    }
    return controlBoardList;
}

std::map<std::string,int> getControlBoardIdsMap(const std::vector<std::string> & controlBoardNames)
{
    std::map<std::string,int> controlBoardIds;
    for(int ctrlBoard=0; ctrlBoard < (int)controlBoardNames.size(); ctrlBoard++)
    {
        controlBoardIds[controlBoardNames[ctrlBoard]] = ctrlBoard;
    }
    return controlBoardIds;
}

bool loadJointsControlBoardFromConfig(yarp::os::Property & wbi_yarp_properties,
                                      const wbi::IDList & jointIdList,
                                      std::vector<std::string> & controlBoardNames,
                                      std::vector< std::pair<int,int> > & controlBoardAxisList)
{
    yarp::os::Bottle & joints_config = getWBIYarpJointsOptions(wbi_yarp_properties);

    //First check that all the joint in the jointIdList an appropriate controlboard mapping is defined
    controlBoardNames.clear();
    if (!appendNewControlBoardsToVector(joints_config,jointIdList,controlBoardNames))
    {
        return false;
    }

    std::map<std::string,int> controlBoardIds = getControlBoardIdsMap(controlBoardNames);


    //Resizing structures
    controlBoardAxisList.resize(jointIdList.size());
    //totalControlledJointsInControlBoard.resize(controlBoardNames.size(),0);
    getControlBoardAxisList(joints_config,jointIdList,controlBoardNames,controlBoardAxisList);


    //All elements in the jointIdList have a mapping to an axis of a controlboard
    //we have to save this mapping
    for (int wbi_jnt = 0; wbi_jnt < (int)jointIdList.size(); wbi_jnt++)
    {
        wbi::ID wbi_jnt_name;
        jointIdList.indexToID(wbi_jnt,wbi_jnt_name);

        yarp::os::Bottle * ctrlBoard_mapping = joints_config.find(wbi_jnt_name.toString().c_str()).asList();

        std::string controlboard_name = ctrlBoard_mapping->get(0).asString().c_str();
        int controlboard_jnt_axis = ctrlBoard_mapping->get(1).asInt();
        int controlboard_wbi_id   = controlBoardIds[controlboard_name];

        //totalControlledJointsInControlBoard[controlboard_wbi_id]++;
        controlBoardAxisList[wbi_jnt] = std::pair<int,int>(controlboard_wbi_id,controlboard_jnt_axis);
    }

    return true;
}

bool loadSensorPortsFromConfig(yarp::os::Property & wbi_yarp_properties,
                               const wbi::IDList & sensorIdList,
                               std::vector<std::string> & ports,
                               const std::string group_name)
{
    yarp::os::Bottle ports_list = wbi_yarp_properties.findGroup(group_name);
    if( ports_list.isNull() || ports_list.size() == 0 ) {
        ports.resize(0);
        return true;
    }

    ports.resize(sensorIdList.size());


    for(int sensor_index = 0; sensor_index < (int)sensorIdList.size(); sensor_index++ ) {
        wbi::ID sensorID;
        sensorIdList.indexToID(sensor_index,sensorID);
        yarp::os::Value & port = ports_list.find(sensorID.toString());
        if( (port.isNull()) || !(port.isString()) ) {
            yError() << "yarpWbi::loadSensorPortsFromConfig error: " << ports_list.toString() <<
                         " returned an error when search for port of sensor " << sensorID.toString();
            return false;
        }
        std::string port_name = port.asString().c_str();
        ports[sensor_index] = port_name;
    }
    return true;
}



bool loadFTSensorPortsFromConfig(yarp::os::Property & wbi_yarp_properties,
                                 wbi::IDList & sensorIdList,
                                 std::vector<std::string> & ports)
{
    return loadSensorPortsFromConfig(wbi_yarp_properties,
                                     sensorIdList,
                                     ports,
                                     "WBI_YARP_FT_PORTS");
}

bool loadIMUSensorPortsFromConfig(yarp::os::Property & wbi_yarp_properties,
                                 wbi::IDList & sensorIdList,
                                 std::vector<std::string> & ports)
{
    return loadSensorPortsFromConfig(wbi_yarp_properties,
                                     sensorIdList,
                                     ports,
                                     "WBI_YARP_IMU_PORTS");
}


bool loadIdListsFromConfigRecursiveHelper(std::string & requested_list,
                                          std::vector<std::string> & lists_names_stack,
                                          std::vector<std::string> & id_list_elements,
                                          yarp::os::Bottle & list_bots)
{
    const yarp::os::Bottle * requested_list_bot = list_bots.find(requested_list).asList();
    //std::cout << "[INFO] Requested list bot: " << requested_list_bot->toString() << std::endl;
    if( requested_list_bot == NULL )
    {
        yError() << "loadIdListFromConfig error: requested list " << requested_list << " not found in configuration file ";
        return false;
    }
    //This is needed to check for a circular inclusion in the lists
    if (std::find(lists_names_stack.begin(), lists_names_stack.end(), requested_list) != lists_names_stack.end())
    {
        // List already include once, error
        yError() << "loadIdListFromConfig error: requested list " << requested_list << " is duplicated "
                  << " inside of parent list " << lists_names_stack[0];
        return false;
    }

    lists_names_stack.push_back(requested_list);

    for( int el = 0; el < requested_list_bot->size(); el++ )
    {
        std::string id_to_add = requested_list_bot->get(el).asString();
        if( !(list_bots.find(id_to_add).isNull()) )
        {
            //Composite list, expand the element
            bool ok = loadIdListsFromConfigRecursiveHelper(id_to_add,lists_names_stack,id_list_elements,list_bots);

            if( !ok )
            {
                return false;
            }
        }
        else
        {
            id_list_elements.push_back(id_to_add);
        }
    }


    return true;
}

bool loadIdListFromConfig(std::string requested_list,
                          const yarp::os::Searchable & wbi_yarp_properties,
                          wbi::IDList & requestedIdList,
                          std::string list_group,
                          bool verbose
                         )
{
    yarp::os::ConstString list_group_cstr = list_group;
    yarp::os::Bottle & list_bot = wbi_yarp_properties.findGroup(list_group_cstr);
    yarp::os::ConstString requested_list_cstr = requested_list;
    yarp::os::Value & requested_list_val = list_bot.find(requested_list_cstr);
    yarp::os::Bottle * requested_list_bot = requested_list_val.asList();

    if( requested_list_val.isNull() || (requested_list_bot == NULL) )
    {
        if( verbose )
            yError() << "loadIdListFromConfig error: requested list " << requested_list << " not found";
        return false;
    }

    std::vector<std::string> ids;
    std::vector<std::string> lists_names_stack;

    bool ret = loadIdListsFromConfigRecursiveHelper(requested_list,lists_names_stack,ids,list_bot);

    if( !ret )
    {
        if( verbose )
            yError() << "loadIdListFromConfig error: requested list " << requested_list << " is malformed";
        return false;
    }

    requestedIdList = wbi::IDList();

    for(int id = 0; id < (int)ids.size(); id++ )
    {
        requestedIdList.addID(wbi::ID(ids[id]));
    }

    return ret;
}

std::string stringFromInt(int num)
{
    std::stringstream numStream;
    numStream << num;
    return numStream.str();
}


bool pidFromBottleDescription(const yarp::os::Bottle &bottle,
                              yarp::dev::Pid &pid)
{
    for (int i = 1; i < bottle.size(); ++i) {
        if (!bottle.get(i).isList()) continue;
        yarp::os::Bottle *gains = bottle.get(i).asList();
        if (gains->size() == 2 && gains->get(0).isString() && gains->get(1).isDouble()) {
            std::string gainType = gains->get(0).asString();
            double value = gains->get(1).asDouble();

            if (gainType == "kp") {
                pid.setKp(value);
            } else if (gainType == "kd") {
                pid.setKd(value);
            } else if (gainType == "ki") {
                pid.setKi(value);
            } else if (gainType == "stic_up") {
                pid.setStictionValues(value, pid.stiction_down_val);
            } else if (gainType == "stic_down") {
                pid.setStictionValues(pid.stiction_up_val, value);
            }
        }
    }
    return true;
}


bool motorTorqueParametersFromBottleDescription(const yarp::os::Bottle &bottle,
                                                yarp::dev::MotorTorqueParameters& motorParameters)
{
    for (int i = 1; i < bottle.size(); ++i) {
        if (!bottle.get(i).isList()) continue;
        yarp::os::Bottle *gains = bottle.get(i).asList();
        if (gains->size() == 2 && gains->get(0).isString() && gains->get(1).isDouble()) {
            std::string gainType = gains->get(0).asString();
            double value = gains->get(1).asDouble();

            if (gainType == "bmef") {
                motorParameters.bemf = value;
            } else if (gainType == "ktau") {
                motorParameters.ktau = value;
            }
        }
    }
    return true;
}

    bool idListFromParsedConfigurationOption(const yarp::os::Property& wbiConfigProp,
                                             const yarp::os::Value& listSpecification,
                                             wbi::IDList& idList) {
        // There are two ways of specifying the list throuth the mask parameter:
        // either the specified parameter is a list (in the sense of YARP configuration file list,
        // so something like (joint1,joint2,joint3) ) and it that case it is
        // considered to by directly the list of joints to load, or otherwise
        // the wbi list is just a string, and it is considered the name of the list
        // in the yarpWholeBodyInterface.ini file

        // Reset the idList
        idList.removeAllIDs();

        if (listSpecification.isList()) {
            // If the list param is a (YARP) list, load the IDList from it
            for (int jnt = 0; jnt < listSpecification.asList()->size(); jnt++)
            {
                yarp::os::ConstString subElementName = listSpecification.asList()->get(jnt).asString();
                wbi::IDList sublist;
                if (yarpWbi::loadIdListFromConfig(subElementName, wbiConfigProp, sublist)) {
                    idList.addIDList(sublist);
                } else {
                    idList.addID(wbi::ID(subElementName.c_str()));
                }
            }
        } else if (listSpecification.isString()) {
            // Otherwise consider the list to be a
            if (!yarpWbi::loadIdListFromConfig(listSpecification.asString(), wbiConfigProp, idList)) {
                return false;
            }
        }
        return true;
    }

}
