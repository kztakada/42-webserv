#include "server/request_router/request_router.hpp"

namespace server
{
using utils::result::Result;
using http::HttpStatus;

RequestRouter::RequestRouter(const ServerConfig& config) : servers_()
{
    servers_.reserve(config.servers.size());
    for (size_t i = 0; i < config.servers.size(); ++i)
    {
        servers_.push_back(VirtualServer(config.servers[i]));
    }
}

RequestRouter::~RequestRouter() {}

Result<LocationRouting> RequestRouter::route(
    const http::HttpRequest& request, const IPAddress& server_ip,
    const PortType& server_port) const
{
    if (servers_.empty())
    {
        return Result<LocationRouting>(
            utils::result::ERROR, "no virtual servers configured");
    }

    Result<ResolvedPath> resolved =
        ResolvedPath::create(request);
    if (resolved.isError())
    {
        return LocationRouting(NULL, NULL, std::string(), std::string(),
            request.getMinorVersion(), HttpStatus::BAD_REQUEST);
    }

    Result<std::string> normalized_path =
        resolved.unwrap().resolveDotSegmentsOrError();
    if (normalized_path.isError())
    {
        return LocationRouting(NULL, NULL, std::string(),
            resolved.unwrap().getHost(), resolved.unwrap().getMinorVersion(),
            HttpStatus::BAD_REQUEST);
    }

    const VirtualServer* vserver = selectVirtualServer(
        server_ip, server_port, resolved.unwrap().getHost());
    if (!vserver)
    {
        return Result<LocationRouting>(utils::result::ERROR,
            "no virtual server for given listen endpoint");
    }
    const LocationDirective* location =
        selectLocationByPath(normalized_path.unwrap(), vserver);
    return LocationRouting(vserver, location, normalized_path.unwrap(),
        resolved.unwrap().getHost(), resolved.unwrap().getMinorVersion(),
        HttpStatus::OK);
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
