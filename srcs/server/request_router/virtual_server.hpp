#ifndef WEBSERV_SERVER_VIRTUAL_SERVER_HPP_
#define WEBSERV_SERVER_VIRTUAL_SERVER_HPP_

#include <string>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/virtual_server_conf.hpp"
#include "server/request_router/location_directive.hpp"

namespace server
{

class VirtualServer
{
   private:
    const VirtualServerConf conf_;
    const std::vector<LocationDirective> locations;

   public:
    // Configからの変換用コンストラクタ
    explicit VirtualServer(const VirtualServerConf& conf);

    // Getter（データへのアクセス）
    const IPAddress& getListenIp() const { return conf_.listen_ip; }
    const PortType& getListenPort() const { return conf_.listen_port; }

    // ビジネスロジック
    bool isServerNameIncluded(const std::string& server_name) const
    {
        return conf_.server_names.find(server_name) != conf_.server_names.end();
    }
    // path を元に適切な LocationDirective を返す｡
    // path に該当する LocationDirective がない場合はNULLを返す｡
    const LocationDirective* findLocationByPath(const std::string& path) const
    {
        for (size_t i = 0; i < locations.size(); ++i)
        {
            if (locations[i].isMatchPattern(path))
            {
                return &locations[i];
            }
        }
        return NULL;
    }
};

}  // namespace server

#endif