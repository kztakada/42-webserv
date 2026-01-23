#ifndef WEBSERV_VIRTUAL_SERVER_CONF_HPP_
#define WEBSERV_VIRTUAL_SERVER_CONF_HPP_

#include <set>
#include <string>
#include <vector>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/location_directive_conf.hpp"

namespace server
{

// 仮想サーバーの設定. Nginx の server ブロックに相当.
struct VirtualServerConf
{
    IPAddress listen_ip;
    PortType listen_port;
    std::set<std::string> server_names;
    std::vector<LocationDirectiveConf> locations;

    VirtualServerConf() : listen_ip("0.0.0.0"), listen_port("") {}

    bool isValid() const
    {
        if (listen_port.empty() || locations.empty())
        {
            return false;
        }
        for (size_t i = 0; i < locations.size(); ++i)
        {
            if (!locations[i].isValid())
            {
                return false;
            }
        }
        return true;
    }
};

}  // namespace server

#endif