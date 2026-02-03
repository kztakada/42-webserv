#include "server/request_router/virtual_server.hpp"

#include "http/http_method.hpp"

namespace server
{

VirtualServer::VirtualServer(const VirtualServerConf& conf)
    : conf_(conf), locations_()
{
    locations_.reserve(conf_.locations.size());
    for (size_t i = 0; i < conf_.locations.size(); ++i)
    {
        LocationDirectiveConf merged = conf_.locations[i];

        if (!merged.has_root_dir)
        {
            merged.root_dir = conf_.root_dir;
        }
        if (!merged.has_index_pages)
        {
            merged.index_pages = conf_.index_pages;
        }
        if (!merged.has_client_max_body_size)
        {
            merged.client_max_body_size = conf_.client_max_body_size;
        }

        if (!merged.has_allowed_methods)
        {
            merged.allowed_methods.insert(http::HttpMethod::GET);
        }

        // error_page は server の設定をベースにして location を上書き
        if (!conf_.error_pages.empty())
        {
            ErrorPagesMap merged_error_pages = conf_.error_pages;
            for (ErrorPagesMap::const_iterator it = merged.error_pages.begin();
                it != merged.error_pages.end(); ++it)
            {
                merged_error_pages[it->first] = it->second;
            }
            merged.error_pages.swap(merged_error_pages);
        }

        locations_.push_back(LocationDirective(merged));
    }
}

bool VirtualServer::listensOn(
    const IPAddress& listen_ip, const PortType& listen_port) const
{
    for (size_t i = 0; i < conf_.listens.size(); ++i)
    {
        const Listen& candidate = conf_.listens[i];
        if (candidate.port.toString() != listen_port.toString())
        {
            continue;
        }
        if (candidate.host_ip.isWildcard() ||
            candidate.host_ip.toString() == listen_ip.toString())
        {
            return true;
        }
    }
    return false;
}

bool VirtualServer::isServerNameIncluded(const std::string& server_name) const
{
    return conf_.server_names.find(server_name) != conf_.server_names.end();
}

const LocationDirective* VirtualServer::findLocationByPath(
    const std::string& path) const
{
    const LocationDirective* best = NULL;
    size_t best_pattern_len = 0;
    for (size_t i = 0; i < locations_.size(); ++i)
    {
        if (!locations_[i].isMatchPattern(path))
        {
            continue;
        }
        const size_t pattern_len = locations_[i].pathPatternLength();
        if (!best || pattern_len > best_pattern_len)
        {
            best = &locations_[i];
            best_pattern_len = pattern_len;
        }
    }
    return best;
}

bool VirtualServer::tryGetErrorPagePath(
    const http::HttpStatus& status, std::string* out_path) const
{
    ErrorPagesMap::const_iterator it = conf_.error_pages.find(status);
    if (it == conf_.error_pages.end())
    {
        if (out_path)
            out_path->clear();
        return false;
    }
    if (out_path)
    {
        *out_path = it->second;
    }
    return true;
}

}  // namespace server
