#include "server/config/virtual_server_conf.hpp"

#include <map>
#include <set>

#include "utils/path.hpp"
#include "utils/result.hpp"

namespace server
{

using utils::result::ERROR;
using utils::result::Result;

static bool containsNul_(const std::string& s)
{
    return s.find('\0') != std::string::npos;
}

static bool isValidUriOrUrlTarget_(const std::string& target)
{
    if (target.empty() || containsNul_(target))
    {
        return false;
    }
    if (target[0] == '/')
    {
        return true;
    }
    if (target.compare(0, 7, "http://") == 0)
    {
        return true;
    }
    if (target.compare(0, 8, "https://") == 0)
    {
        return true;
    }
    return false;
}

VirtualServerConf::VirtualServerConf()
    : listens(),
      root_dir(),
      index_pages(),
      client_max_body_size(1024 * 1024),
      has_client_max_body_size(false),
      error_pages(),
      locations()
{
    index_pages.push_back("index.html");
}

Result<void> VirtualServerConf::appendListen(
    const std::string& listen_ip_str, const std::string& listen_port_str)
{
    if (listen_port_str.empty())
    {
        return Result<void>(ERROR, "listen port is empty");
    }

    IPAddress ip = IPAddress::ipv4Any();
    if (!listen_ip_str.empty())
    {
        Result<IPAddress> parsed_ip =
            IPAddress::parseIpv4Numeric(listen_ip_str);
        if (parsed_ip.isError())
        {
            return Result<void>(ERROR, "listen ip is invalid");
        }
        ip = parsed_ip.unwrap();
    }

    Result<PortType> parsed = PortType::parseNumeric(listen_port_str);
    if (parsed.isError())
    {
        return Result<void>(ERROR, "listen port is invalid");
    }

    return appendListen(Listen(ip, parsed.unwrap()));
}

Result<void> VirtualServerConf::appendListen(const Listen& listen)
{
    if (listen.host_ip.empty())
    {
        return Result<void>(ERROR, "listen ip is empty");
    }
    if (listen.port.empty())
    {
        return Result<void>(ERROR, "listen port is empty");
    }

    for (size_t i = 0; i < listens.size(); ++i)
    {
        if (listens[i].host_ip.toString() == listen.host_ip.toString() &&
            listens[i].port.toString() == listen.port.toString())
        {
            return Result<void>(ERROR, "directive is duplicate: listen");
        }
    }
    listens.push_back(listen);
    return Result<void>();
}

std::vector<Listen> VirtualServerConf::getListens() const
{
    // 1) port ごとに wildcard の有無を先に確定する（順序に依存しない）
    std::map<std::string, bool> wildcard_ports;
    for (size_t i = 0; i < listens.size(); ++i)
    {
        const std::string port = listens[i].port.toString();
        if (port.empty())
        {
            continue;
        }
        if (listens[i].host_ip.isWildcard())
        {
            wildcard_ports[port] = true;
        }
    }

    // 2) 重複解決しながら出力
    std::vector<Listen> out;
    out.reserve(listens.size());

    std::set<std::string> included_pairs;
    std::set<std::string> included_wildcard_ports;

    for (size_t i = 0; i < listens.size(); ++i)
    {
        const Listen& l = listens[i];
        const std::string port = l.port.toString();
        const std::string ip = l.host_ip.toString();
        if (port.empty() || ip.empty())
        {
            continue;
        }

        const bool has_wildcard =
            wildcard_ports.find(port) != wildcard_ports.end();

        if (has_wildcard)
        {
            if (!l.host_ip.isWildcard())
            {
                continue;
            }
            if (included_wildcard_ports.find(port) !=
                included_wildcard_ports.end())
            {
                continue;
            }
            included_wildcard_ports.insert(port);
            out.push_back(l);
            continue;
        }

        const std::string key = ip + ":" + port;
        if (included_pairs.find(key) != included_pairs.end())
        {
            continue;
        }
        included_pairs.insert(key);
        out.push_back(l);
    }

    return out;
}

Result<void> VirtualServerConf::appendServerName(
    const std::string& server_name_str)
{
    server_names.insert(server_name_str);
    return Result<void>();
}

Result<void> VirtualServerConf::setRootDir(const std::string& root_dir_str)
{
    if (!this->root_dir.empty())
    {
        return Result<void>(ERROR, "directive is duplicate: root");
    }
    if (root_dir_str.empty())
    {
        return Result<void>(ERROR, "root path is empty");
    }
    if (containsNul_(root_dir_str))
    {
        return Result<void>(ERROR, "root path contains NUL");
    }
    utils::result::Result<utils::path::PhysicalPath> resolved =
        utils::path::PhysicalPath::resolve(root_dir_str);
    if (resolved.isError())
    {
        return Result<void>(ERROR, "root path is invalid");
    }
    this->root_dir = resolved.unwrap();
    return Result<void>();
}

Result<void> VirtualServerConf::appendIndexPage(const std::string& filename_str)
{
    if (filename_str.empty())
    {
        return Result<void>(ERROR, "index page path is empty");
    }
    if (containsNul_(filename_str))
    {
        return Result<void>(ERROR, "index page path contains NUL");
    }
    index_pages.push_back(filename_str);
    return Result<void>();
}

Result<void> VirtualServerConf::setClientMaxBodySize(unsigned long size)
{
    if (has_client_max_body_size)
    {
        return Result<void>(
            ERROR, "directive is duplicate: client_max_body_size");
    }
    if (size > static_cast<unsigned long>(INT_MAX))
    {
        return Result<void>(ERROR, "client_max_body_size exceeds INT_MAX");
    }
    has_client_max_body_size = true;
    client_max_body_size = size;
    return Result<void>();
}

Result<void> VirtualServerConf::appendErrorPage(
    http::HttpStatus status, const std::string& page_url_str)
{
    if (!status.isError())
    {
        return Result<void>(ERROR, "error_page status must be 4xx/5xx");
    }
    if (page_url_str.empty())
    {
        return Result<void>(ERROR, "error_page path is empty");
    }
    if (containsNul_(page_url_str))
    {
        return Result<void>(ERROR, "error_page path contains NUL");
    }
    if (!isValidUriOrUrlTarget_(page_url_str))
    {
        return Result<void>(ERROR, "error_page path is invalid");
    }
    error_pages[status] = page_url_str;
    return Result<void>();
}

Result<void> VirtualServerConf::appendLocation(
    const LocationDirectiveConf& location)
{
    if (!location.isValid())
    {
        return Result<void>(ERROR, "location config is invalid");
    }
    locations.push_back(location);
    return Result<void>();
}

bool VirtualServerConf::isValid() const
{
    if (listens.empty() || root_dir.empty())
    {
        return false;
    }
    for (size_t i = 0; i < listens.size(); ++i)
    {
        if (listens[i].host_ip.empty() || listens[i].port.empty())
        {
            return false;
        }
    }
    if (client_max_body_size > INT_MAX)
    {
        return false;
    }
    for (size_t i = 0; i < locations.size(); ++i)
    {
        if (!locations[i].isValid())
        {
            return false;
        }
    }
    return true;
}

}  // namespace server
