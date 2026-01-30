#include "server/config/server_config.hpp"

#include "utils/result.hpp"

namespace server
{

using utils::result::ERROR;
using utils::result::Result;

Result<void> ServerConfig::appendServer(const VirtualServerConf& server)
{
    if (!server.isValid())
    {
        return Result<void>(ERROR, "virtual server config is invalid");
    }
    servers.push_back(server);
    return Result<void>();
}

bool ServerConfig::isValid() const
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

}  // namespace server
