#ifndef WEBSERV_SERVER_REQUEST_ROUTER_HPP_
#define WEBSERV_SERVER_REQUEST_ROUTER_HPP_

#include <string>
#include <vector>

#include "http/http_request.hpp"
#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/server_config.hpp"
#include "server/request_router/location_directive.hpp"
#include "server/request_router/virtual_server.hpp"

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
    // @return 見つからない場合は NULL
    const LocationDirective* route(const http::HttpRequest& request,
        const IPAddress& server_ip, const PortType& server_port) const;

   private:
    std::vector<VirtualServer> servers;
    // Virtual Server選択
    // listen_port と server_name を元に適切なバーチャルサーバを返す｡
    // 該当するバーチャルサーバがない場合はNULLを返す｡
    const VirtualServer* selectVirtualServer(const IPAddress& listen_ip,
        const PortType& listen_port, const std::string& server_name) const;

    // すべてのバーチャルサーバを保持するvectorへの参照を返す｡
    const std::vector<VirtualServer>& getVirtualServers() const;

    // Location選択
    const LocationDirective* selectLocation(
        const http::HttpRequest& request, const VirtualServer* vserver) const;

    // server_nameマッチング
    bool matchServerName(const std::string& host_header,
        const std::vector<std::string>& server_names) const;
};

}  // namespace server
#endif