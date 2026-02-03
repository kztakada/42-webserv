#include "server/config/server_config.hpp"

#include <map>
#include <set>

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

std::vector<Listen> ServerConfig::getListens() const
{
    // 1) 各 server の listen を集約（server内重複解決済みのものを使う）
    std::vector<Listen> all;
    for (size_t i = 0; i < servers.size(); ++i)
    {
        const std::vector<Listen> ls = servers[i].getListens();
        for (size_t j = 0; j < ls.size(); ++j)
            all.push_back(ls[j]);
    }

    // 2) port ごとに wildcard の有無を先に確定する（順序に依存しない）
    std::map<std::string, bool> wildcard_ports;
    for (size_t i = 0; i < all.size(); ++i)
    {
        const std::string port = all[i].port.toString();
        if (port.empty())
            continue;
        if (all[i].host_ip.isWildcard())
            wildcard_ports[port] = true;
    }

    // 3) 重複解決しながら出力
    std::vector<Listen> out;
    out.reserve(all.size());

    std::set<std::string> included_pairs;
    std::set<std::string> included_wildcard_ports;

    for (size_t i = 0; i < all.size(); ++i)
    {
        const Listen& l = all[i];
        const std::string port = l.port.toString();
        const std::string ip = l.host_ip.toString();
        if (port.empty() || ip.empty())
            continue;

        const bool has_wildcard =
            wildcard_ports.find(port) != wildcard_ports.end();

        if (has_wildcard)
        {
            if (!l.host_ip.isWildcard())
                continue;
            if (included_wildcard_ports.find(port) !=
                included_wildcard_ports.end())
                continue;
            included_wildcard_ports.insert(port);
            out.push_back(l);
            continue;
        }

        const std::string key = ip + ":" + port;
        if (included_pairs.find(key) != included_pairs.end())
            continue;
        included_pairs.insert(key);
        out.push_back(l);
    }

    return out;
}

}  // namespace server
