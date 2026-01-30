#ifndef WEBSERV_SERVER_REQUEST_ROUTER_HPP_
#define WEBSERV_SERVER_REQUEST_ROUTER_HPP_

#include <cstddef>
#include <string>
#include <vector>

#include "http/http_request.hpp"
#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/server_config.hpp"
#include "server/request_router/location_directive.hpp"
#include "server/request_router/location_routing.hpp"
#include "server/request_router/resolved_path.hpp"
#include "server/request_router/virtual_server.hpp"
#include "utils/result.hpp"

/* RequestRouterの責務
1. Virtual Server選択: Host header と server_name のマッチング
2. Location選択: URIパスと location のマッチング
3. 設定を返す: 適切な LocationConf を返す
*/

namespace server
{

class RequestRouter
{
   public:
    RequestRouter(const ServerConfig& config);
    ~RequestRouter();

    // リクエストに対応するLocationConfを取得
    // @return 内部エラーは Result(ERROR, msg)
    //         該当ロケーションがない場合は OK で location == NULL
    utils::result::Result<LocationRouting> route(
        const http::HttpRequest& request, const IPAddress& server_ip,
        const PortType& server_port) const;

   private:
    std::vector<VirtualServer> servers_;

    // Virtual Server選択
    // listen_port と server_name を元に適切なバーチャルサーバを返す｡
    // 該当するバーチャルサーバがない場合はNULLを返す｡
    const VirtualServer* selectVirtualServer(const IPAddress& listen_ip,
        const PortType& listen_port, const std::string& server_name) const;

    // Location選択
    const LocationDirective* selectLocationByPath(
        const std::string& path, const VirtualServer* vserver) const;
};

}  // namespace server
#endif