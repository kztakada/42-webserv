#include "server/config/virtual_server_conf.hpp"

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
    : listen_ip(IPAddress::ipv4Any()),
      listen_port(),
      root_dir(),
      index_pages(),
      client_max_body_size(1024 * 1024),
      has_client_max_body_size(false),
      error_pages(),
      locations()
{
    index_pages.push_back("index.html");
}

Result<void> VirtualServerConf::setListenIp(const std::string& listen_ip_str)
{
    if (!this->listen_port.empty())
    {
        return Result<void>(ERROR, "directive is duplicate: listen");
    }
    if (listen_ip_str.empty())
    {
        this->listen_ip = IPAddress::ipv4Any();
        return Result<void>();
    }

    Result<IPAddress> parsed = IPAddress::parseIpv4Numeric(listen_ip_str);
    if (parsed.isError())
    {
        return Result<void>(ERROR, "listen ip is invalid");
    }
    this->listen_ip = parsed.unwrap();
    return Result<void>();
}

Result<void> VirtualServerConf::setListenPort(
    const std::string& listen_port_str)
{
    if (!this->listen_port.empty())
    {
        return Result<void>(ERROR, "directive is duplicate: listen");
    }
    if (listen_port_str.empty())
    {
        return Result<void>(ERROR, "listen port is empty");
    }

    Result<PortType> parsed = PortType::parseNumeric(listen_port_str);
    if (parsed.isError())
    {
        return Result<void>(ERROR, "listen port is invalid");
    }
    this->listen_port = parsed.unwrap();
    return Result<void>();
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
    if (listen_port.empty() || root_dir.empty())
    {
        return false;
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
