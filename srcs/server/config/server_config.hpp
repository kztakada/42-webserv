#ifndef WEBSERV_SERVER_CONFIG_HPP_
#define WEBSERV_SERVER_CONFIG_HPP_

#include <string>
#include <vector>

#include "server/config/virtual_server_conf.hpp"

namespace server
{

struct ServerConfig
{
    std::vector<VirtualServerConf> servers;

    ServerConfig() : servers() {}
    bool isValid() const
    {
        if (servers.empty())
        {
            return false;
        }
        for (size_t i = 0; i < servers.size(); ++i)
        {
            if (!servers[i].isValid())
            {
                return false;
            }
        }
        return true;
    }
};

}  // namespace server

#endif