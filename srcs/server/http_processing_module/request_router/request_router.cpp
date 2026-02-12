#include "server/http_processing_module/request_router/request_router.hpp"

namespace server
{
using http::HttpStatus;
using utils::result::Result;

RequestRouter::RequestRouter(const ServerConfig& config) : servers_()
{
    servers_.reserve(config.servers.size());
    for (size_t i = 0; i < config.servers.size(); ++i)
    {
        servers_.push_back(VirtualServer(config.servers[i]));
    }
}

RequestRouter::~RequestRouter() {}

Result<LocationRouting> RequestRouter::route(const http::HttpRequest& request,
    const IPAddress& server_ip, const PortType& server_port) const
{
    if (servers_.empty())
    {
        return Result<LocationRouting>(
            utils::result::ERROR, "no virtual servers configured");
    }

    Result<ResolvedRequestContext> ctx =
        ResolvedRequestContext::create(request);
    if (ctx.isError())
    {
        const VirtualServer* vserver =
            selectVirtualServer(server_ip, server_port, std::string());
        if (!vserver)
        {
            return Result<LocationRouting>(utils::result::ERROR,
                "no virtual server for given listen endpoint");
        }
        return LocationRouting(vserver, NULL,
            ResolvedRequestContext::createForBadRequest(request), request,
            HttpStatus::BAD_REQUEST);
    }
    ResolvedRequestContext resolved = ctx.unwrap();

    Result<void> normalized = resolved.resolveDotSegmentsOrError();
    if (normalized.isError())
    {
        const VirtualServer* vserver =
            selectVirtualServer(server_ip, server_port, resolved.getHost());
        if (!vserver)
        {
            return Result<LocationRouting>(utils::result::ERROR,
                "no virtual server for given listen endpoint");
        }
        return LocationRouting(
            vserver, NULL, resolved, request, HttpStatus::BAD_REQUEST);
    }

    const VirtualServer* vserver =
        selectVirtualServer(server_ip, server_port, resolved.getHost());
    if (!vserver)
    {
        return Result<LocationRouting>(utils::result::ERROR,
            "no virtual server for given listen endpoint");
    }
    const LocationDirective* location =
        selectLocationByPath(resolved.getRequestPath(), vserver);

    // サーバーが実装していないメソッド（例: HEAD, PUT）も、
    // リクエスト自体が文法的に正しければ 405 + Allow で返す仕様にする。
    // そのためメソッドのサポート可否はここでは判定しない。
    return LocationRouting(
        vserver, location, resolved, request, HttpStatus::OK);
}

const VirtualServer* RequestRouter::selectVirtualServer(
    const IPAddress& listen_ip, const PortType& listen_port,
    const std::string& server_name) const
{
    const VirtualServer* default_server = NULL;
    for (size_t i = 0; i < servers_.size(); ++i)
    {
        if (!servers_[i].listensOn(listen_ip, listen_port))
        {
            continue;
        }
        if (!default_server)
        {
            default_server = &servers_[i];
        }
        if (!server_name.empty() &&
            servers_[i].isServerNameIncluded(server_name))
        {
            return &servers_[i];
        }
    }
    return default_server;
}

const LocationDirective* RequestRouter::selectLocationByPath(
    const std::string& path, const VirtualServer* vserver) const
{
    if (!vserver)
    {
        return NULL;
    }
    return vserver->findLocationByPath(path);
}

}  // namespace server
