#include "server/request_router/location_directive.hpp"

namespace server
{

LocationDirective::LocationDirective(const LocationDirectiveConf& conf)
    : conf_(conf)
{
}

unsigned long LocationDirective::clientMaxBodySize() const
{
    return conf_.client_max_body_size;
}

const FilePath& LocationDirective::rootDir() const { return conf_.root_dir; }

bool LocationDirective::isBackwardSearch() const
{
    return conf_.is_backward_search;
}

bool LocationDirective::isCgiEnabled() const
{
    return !conf_.cgi_extensions.empty();
}

bool LocationDirective::isAutoIndexEnabled() const { return conf_.auto_index; }

bool LocationDirective::hasRedirect() const
{
    return !conf_.redirect_url.empty();
}

const std::string& LocationDirective::redirectTarget() const
{
    return conf_.redirect_url;
}

http::HttpStatus LocationDirective::redirectStatus() const
{
    return conf_.redirect_status;
}

bool LocationDirective::hasUploadStore() const
{
    return !conf_.upload_store.empty();
}

const FilePath& LocationDirective::uploadStore() const
{
    return conf_.upload_store;
}

bool LocationDirective::tryGetErrorPagePath(
    const http::HttpStatus& status, std::string* out_path) const
{
    ErrorPagesMap::const_iterator it = conf_.error_pages.find(status);
    if (it == conf_.error_pages.end())
    {
        return false;
    }
    if (out_path)
    {
        *out_path = it->second;
    }
    return true;
}

void LocationDirective::chooseCgiExecutorByRequestPath(
    const std::string& uri_path, FilePath* out_executor, std::string* out_ext,
    size_t* out_script_end) const
{
    if (out_script_end)
    {
        *out_script_end = std::string::npos;
    }
    if (out_executor)
    {
        *out_executor = FilePath();
    }
    if (out_ext)
    {
        out_ext->clear();
    }
    if (uri_path.empty())
    {
        return;
    }

    size_t best_end = std::string::npos;
    FilePath best_exec;
    std::string best_ext;

    for (LocationDirectiveConf::CgiExtensionsMap::const_iterator it =
             conf_.cgi_extensions.begin();
        it != conf_.cgi_extensions.end(); ++it)
    {
        const std::string& ext = it->first;
        const FilePath& exec = it->second;

        size_t pos = uri_path.find(ext);
        while (pos != std::string::npos)
        {
            const size_t end = pos + ext.size();
            if (end == uri_path.size() || uri_path[end] == '/')
            {
                if (best_end == std::string::npos || end < best_end)
                {
                    best_end = end;
                    best_exec = exec;
                    best_ext = ext;
                }
                break;
            }
            pos = uri_path.find(ext, pos + 1);
        }
    }

    if (out_script_end)
    {
        *out_script_end = best_end;
    }
    if (best_end == std::string::npos)
    {
        return;
    }
    if (out_executor)
    {
        *out_executor = best_exec;
    }
    if (out_ext)
    {
        *out_ext = best_ext;
    }
}

std::vector<std::string> LocationDirective::buildIndexCandidatePaths(
    const std::string& request_path) const
{
    std::vector<std::string> candidates;
    candidates.reserve(conf_.index_pages.size());
    for (size_t i = 0; i < conf_.index_pages.size(); ++i)
    {
        candidates.push_back(
            joinPath_(getAbsolutePath(request_path), conf_.index_pages[i]));
    }
    return candidates;
}

bool LocationDirective::isMethodAllowed(const http::HttpMethod& method) const
{
    return conf_.allowed_methods.find(method) != conf_.allowed_methods.end();
}

size_t LocationDirective::pathPatternLength() const
{
    return conf_.path_pattern.length();
}

bool LocationDirective::isMatchPattern(const std::string& path) const
{
    if (conf_.is_backward_search)
    {
        return backwardMatch_(path, conf_.path_pattern);
    }
    return forwardMatch_(path, conf_.path_pattern);
}

std::string LocationDirective::getAbsolutePath(const std::string& path) const
{
    if (conf_.is_backward_search)
    {
        return joinPath_(conf_.root_dir.str(), path);
    }
    return joinPath_(conf_.root_dir.str(), removePathPatternFromPath(path));
}

std::string LocationDirective::removePathPatternFromPath(
    const std::string& path) const
{
    if (conf_.is_backward_search)
    {
        return path;
    }
    return path.substr(conf_.path_pattern.length());
}

bool LocationDirective::isAllowed(const http::HttpMethod& method) const
{
    return isMethodAllowed(method);
}

bool LocationDirective::forwardMatch_(
    const std::string& path, const std::string& pattern)
{
    if (pattern.empty())
    {
        return true;
    }
    if (path.size() < pattern.size())
    {
        return false;
    }
    return path.compare(0, pattern.size(), pattern) == 0;
}

bool LocationDirective::backwardMatch_(
    const std::string& path, const std::string& pattern)
{
    if (pattern.empty())
    {
        return true;
    }
    if (path.size() < pattern.size())
    {
        return false;
    }
    return path.compare(
               path.size() - pattern.size(), pattern.size(), pattern) == 0;
}

std::string LocationDirective::joinPath_(
    const std::string& base, const std::string& leaf)
{
    if (base.empty())
    {
        return leaf;
    }
    if (leaf.empty())
    {
        return base;
    }
    const bool base_slash = base[base.size() - 1] == '/';
    const bool leaf_slash = leaf[0] == '/';
    if (base_slash && leaf_slash)
    {
        return base + leaf.substr(1);
    }
    if (!base_slash && !leaf_slash)
    {
        return base + "/" + leaf;
    }
    return base + leaf;
}

}  // namespace server
