#ifndef WEBSERV_SERVER_CONFIG_HPP_
#define WEBSERV_SERVER_CONFIG_HPP_

#include <string>
#include <vector>

#include "server/config/virtual_server_conf.hpp"
#include "utils/result.hpp"

namespace server
{

struct ServerConfig
{
    std::vector<VirtualServerConf> servers;

    ServerConfig() : servers() {}
    utils::result::Result<void> appendServer(const VirtualServerConf& server);
    std::vector<Listen> getListens() const;
    bool isValid() const;
};

}  // namespace server

#endif