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

#include "wbiIcub/wbiIcubUtil.h"


namespace wbiIcub
{

bool loadBodyPartsFromConfig(yarp::os::Property & wbi_yarp_properties, std::vector<std::string> & body_parts_vector)
{
        std::cout << "wbiIcub::loadBodyPartsFromConfig : config passed " << wbi_yarp_properties.toString() << std::endl;
        yarp::os::Bottle parts_config = wbi_yarp_properties.findGroup("WBI_YARP_BODY_PARTS");
        const std::string numBodyPartsOption = "numBodyParts";
        if( !parts_config.check(numBodyPartsOption) ) {
            std::cout << "wbiIcub::loadBodyPartsFromConfig error: " << numBodyPartsOption << " option not found" << std::endl;
            return false;
        }
        int numBodyParts = parts_config.find(numBodyPartsOption).asInt();
        std::cout << "wbiIcub::loadBodyPartsFromConfig : Loading body parts: expecting " << numBodyParts << " parts " << std::endl;
        body_parts_vector.resize(numBodyParts);
        for(int bp=0; bp < numBodyParts; bp++ ) {
            std::ostringstream bodyPart_strm;
            bodyPart_strm<<"bodyPart"<<bp;
            std::string bodyPart = bodyPart_strm.str();
            if( ! parts_config.check(bodyPart) ) {
                std::cout << "wbiIcub::loadBodyPartsFromConfig error: " << bodyPart << " name not found" << std::endl;
                return false;
            }
            body_parts_vector[bp] = parts_config.find(bodyPart).asString().c_str();
        }
        std::cout << "wbiIcub::loadBodyPartsFromConfig: Loaded body parts: ";
        for(int i=0; i < body_parts_vector.size(); i++ ) { std::cout << " " << body_parts_vector[i] << std::endl; }
        std::cout << std::endl;
        return true;
}

bool loadReverseTorsoJointsFromConfig(yarp::os::Property & wbi_yarp_properties, bool & reverse_torso_joints)
{
    if(  wbi_yarp_properties.findGroup("WBI_YARP_BODY_PARTS_REMAPPING").check("reverse_torso_joints") ) {
        reverse_torso_joints = true;
    } else {
        reverse_torso_joints = false;
    }
    return true;
}

bool loadSensorPortsFromConfig(yarp::os::Property & wbi_yarp_properties,
                               const std::vector<std::string> & body_parts_vector,
                               std::vector<id_2_PortName> &ports,
                               const std::string group_name)
{
    yarp::os::Bottle ports_list = wbi_yarp_properties.findGroup(group_name);
    if( ports_list.isNull() || ports_list.size() == 0 ) {
        ports.resize(0);
        return true;
    }
    ports.resize(ports_list.size());
    for(int port_id = 0; port_id < ports_list.size(); port_id++ ) {
        yarp::os::Bottle * port = ports_list.get(port_id).asList();
        if( port == NULL || port->size() != 3 ) {
            std::cout << "wbiIcub::loadSensorPortsFromConfig error: " << ports_list.toString() << " has an element malformed element" << std::endl;
            return false;
        }
        std::string bodyPart = port->get(0).asString().c_str();
        int id = port->get(1).asInt();
        std::string port_name = port->get(2).asString().c_str();
        int body_part_index = std::find(body_parts_vector.begin(), body_parts_vector.end(), bodyPart) - body_parts_vector.begin();
        if( body_part_index >= (int)body_parts_vector.size() || body_part_index < 0 ) {
            std::cout << "wbiIcub::loadSensorPortsFromConfig error: bodyPart in " << port->toString() << " not recognized." << std::endl;
            return false;
        }
        id_2_PortName id_port_map;
        id_port_map.id = wbi::LocalId(body_part_index,id);
        id_port_map.portName = port_name;
        ports[port_id] = id_port_map;
    }
    return true;
}

bool loadFTSensorPortsFromConfig(yarp::os::Property & wbi_yarp_properties,
                                 const std::vector<std::string> & body_parts_vector,
                                 std::vector<id_2_PortName> &ft_ports)
{
    return loadSensorPortsFromConfig(wbi_yarp_properties,body_parts_vector,ft_ports,"WBI_YARP_FT_PORTS");
}

bool loadIMUSensorPortsFromConfig(yarp::os::Property & wbi_yarp_properties,
                                      const std::vector<std::string> & body_parts_vector,
                                      std::vector<id_2_PortName> &imu_ports)
{
    return loadSensorPortsFromConfig(wbi_yarp_properties,body_parts_vector,imu_ports,"WBI_YARP_IMU_PORTS");
}

bool loadTreeSerializationFromConfig(yarp::os::Property & wbi_yarp_properties,
                                     KDL::Tree& tree,
                                     KDL::CoDyCo::TreeSerialization& serialization)
{
    std::string dofSerializationParamName = "idyntree_dof_serialization";
    std::string linkSerializationParamName = "idyntree_link_serialization";

    if( !wbi_yarp_properties.check(dofSerializationParamName) ) { return false; }
    if( !wbi_yarp_properties.check(linkSerializationParamName) ) { return false; }

    yarp::os::Bottle * dofs_bot = wbi_yarp_properties.find(dofSerializationParamName).asList();
    yarp::os::Bottle * links_bot = wbi_yarp_properties.find(linkSerializationParamName).asList();

    if( !dofs_bot || !links_bot ) { return false; }

    int serializationDOFs = dofs_bot->size()-1;
    int serializationLinks = links_bot->size()-1;

    std::vector<std::string> links;
    links.resize(serializationLinks);

    std::vector<std::string> dofs;
    dofs.resize(serializationDOFs);

    for(int link=0; link < serializationLinks; link++) {
        links[link] = links_bot[link+1].toString().c_str();
    }

    for(int dof=0; dof < serializationDOFs; dof++) {
        dofs[dof] = dofs_bot[dof+1].toString().c_str();
    }

    serialization = KDL::CoDyCo::TreeSerialization(tree,links,dofs);

    return serialization.is_consistent(tree);
}

bool loadTreePartitionFromConfig(yarp::os::Property & wbi_yarp_properties,
                                 KDL::CoDyCo::TreePartition& partition)
{
    wbi_yarp_properties.find("idyntree_serialization");
    return false;
}

bool openPolyDriver(const std::string &localName, const std::string &robotName, yarp::dev::PolyDriver *&pd, const std::string &bodyPartName)
{
    std::string localPort  = "/" + localName + "/" + bodyPartName;
    std::string remotePort = "/" + robotName + "/" + bodyPartName;
    yarp::os::Property options;
    options.put("robot",robotName.c_str());
    options.put("part",bodyPartName.c_str());
    options.put("device","remote_controlboard");
    options.put("local",localPort.c_str());
    options.put("remote",remotePort.c_str());
#ifdef YARP_INTERACTION_MODE_MOTOR_INTERFACE
    options.put("writeStrict","on");
#endif
    pd = new yarp::dev::PolyDriver(options);
    if(!pd || !(pd->isValid()))
    {
        std::fprintf(stderr,"Problems instantiating the device driver %s\n", bodyPartName.c_str());
        return false;
    }
    return true;
}

}
