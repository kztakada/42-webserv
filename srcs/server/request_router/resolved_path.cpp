#include "server/request_router/resolved_path.hpp"

namespace server
{

ResolvedPath::ResolvedPath() : path_(), host_(), minor_version_(1) {}

utils::result::Result<ResolvedPath> ResolvedPath::create(
    const http::HttpRequest& request)
{
    ResolvedPath out;
    out.minor_version_ = request.getMinorVersion();
    out.host_ = extractHost_(request);

    const std::string& raw_path = request.getPath();
    if (raw_path.empty() || raw_path[0] != '/')
    {
        return utils::result::Result<ResolvedPath>(utils::result::ERROR,
            ResolvedPath(), "request path must start with '/'");
    }
    if (raw_path.find('\0') != std::string::npos)
    {
        return utils::result::Result<ResolvedPath>(
            utils::result::ERROR, ResolvedPath(), "request path contains NUL");
    }
    out.path_ = normalizeSlashes_(raw_path);
    return out;
}

const std::string& ResolvedPath::getPath() const { return path_; }

const std::string& ResolvedPath::getHost() const { return host_; }

int ResolvedPath::getMinorVersion() const { return minor_version_; }

utils::result::Result<std::string> ResolvedPath::resolveDotSegmentsOrError()
    const
{
    std::vector<std::string> segments;
    segments.reserve(16);

    bool ends_with_slash = (path_.size() > 1 && path_[path_.size() - 1] == '/');

    size_t i = 0;
    while (i < path_.size())
    {
        while (i < path_.size() && path_[i] == '/')
        {
            ++i;
        }
        size_t start = i;
        while (i < path_.size() && path_[i] != '/')
        {
            ++i;
        }
        if (start == i)
        {
            break;
        }
        const std::string seg = path_.substr(start, i - start);
        if (seg == "." || seg.empty())
        {
            continue;
        }
        if (seg == "..")
        {
            if (segments.empty())
            {
                return utils::result::Result<std::string>(utils::result::ERROR,
                    std::string(), "request path escapes root by '..'");
            }
            segments.pop_back();
            continue;
        }
        segments.push_back(seg);
    }

    std::string out("/");
    for (size_t j = 0; j < segments.size(); ++j)
    {
        out += segments[j];
        if (j + 1 < segments.size())
        {
            out += "/";
        }
    }
    if (ends_with_slash && out.size() > 1)
    {
        out += "/";
    }
    return out;
}

std::string ResolvedPath::extractHost_(const http::HttpRequest& request)
{
    utils::result::Result<const std::vector<std::string>&> host_values =
        request.getHeader("Host");
    if (host_values.isError() || host_values.unwrap().empty())
    {
        return std::string();
    }
    std::string host = host_values.unwrap()[0];
    std::string::size_type colon = host.find(':');
    if (colon != std::string::npos)
    {
        host = host.substr(0, colon);
    }
    return host;
}

std::string ResolvedPath::normalizeSlashes_(const std::string& path)
{
    std::string out;
    out.reserve(path.size());
    bool prev_slash = false;
    for (size_t i = 0; i < path.size(); ++i)
    {
        const char c = path[i];
        if (c == '/')
        {
            if (!prev_slash)
            {
                out += c;
                prev_slash = true;
            }
            continue;
        }
        prev_slash = false;
        out += c;
    }
    if (out.empty())
    {
        return std::string("/");
    }
    return out;
}

}  // namespace server
