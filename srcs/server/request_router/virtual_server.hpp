#ifndef WEBSERV_SERVER_VIRTUAL_SERVER_HPP_
#define WEBSERV_SERVER_VIRTUAL_SERVER_HPP_

#include <string>
#include <vector>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/virtual_server_conf.hpp"
#include "server/request_router/location_directive.hpp"

namespace server
{

class VirtualServer
{
   private:
    VirtualServerConf conf_;
    std::vector<LocationDirective> locations_;

   public:
    // Configからの変換用コンストラクタ
    explicit VirtualServer(const VirtualServerConf& conf);

    // ビジネスロジック
    bool listensOn(const IPAddress& listen_ip, const PortType& listen_port) const;

    bool isServerNameIncluded(const std::string& server_name) const;

    // path を元に適切な LocationDirective を返す｡
    // path に該当する LocationDirective がない場合はNULLを返す｡
    const LocationDirective* findLocationByPath(const std::string& path) const;
};

}  // namespace server

#endif