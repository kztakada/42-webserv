#include "server/request_router/resolved_request_context.hpp"

namespace server
{

ResolvedRequestContext::ResolvedRequestContext()
    : request_(NULL), request_path_(), host_(), minor_version_(1)
{
}

utils::result::Result<ResolvedRequestContext> ResolvedRequestContext::create(
    const http::HttpRequest& request)
{
    ResolvedRequestContext out;
    out.request_ = &request;
    out.minor_version_ = request.getMinorVersion();
    out.host_ = extractHost_(request);

    const std::string& raw_path = request.getPath();
    if (raw_path.empty() || raw_path[0] != '/')
    {
        return utils::result::Result<ResolvedRequestContext>(
            utils::result::ERROR, ResolvedRequestContext(),
            "request path must start with '/'");
    }
    if (raw_path.find('\0') != std::string::npos)
    {
        return utils::result::Result<ResolvedRequestContext>(
            utils::result::ERROR, ResolvedRequestContext(),
            "request path contains NUL");
    }

    out.request_path_ = normalizeSlashes_(raw_path);
    return out;
}

ResolvedRequestContext ResolvedRequestContext::createForBadRequest(
    const http::HttpRequest& request)
{
    ResolvedRequestContext out;
    out.request_ = &request;
    out.minor_version_ = request.getMinorVersion();
    // 互換のため、Host はここでは抽出しない。
    out.host_ = std::string();
    out.request_path_ = std::string();
    return out;
}

const std::string& ResolvedRequestContext::getRequestPath() const
{
    return request_path_;
}

const std::string& ResolvedRequestContext::getHost() const { return host_; }

int ResolvedRequestContext::getMinorVersion() const { return minor_version_; }

const http::HttpRequest* ResolvedRequestContext::getRequest() const
{
    return request_;
}

utils::result::Result<void> ResolvedRequestContext::resolveDotSegmentsOrError()
{
    std::vector<std::string> segments;
    segments.reserve(16);

    const bool ends_with_slash =
        (request_path_.size() > 1 &&
            request_path_[request_path_.size() - 1] == '/');

    size_t i = 0;
    while (i < request_path_.size())
    {
        while (i < request_path_.size() && request_path_[i] == '/')
        {
            ++i;
        }
        size_t start = i;
        while (i < request_path_.size() && request_path_[i] != '/')
        {
            ++i;
        }
        if (start == i)
        {
            break;
        }
        const std::string seg = request_path_.substr(start, i - start);
        if (seg == "." || seg.empty())
        {
            continue;
        }
        if (seg == "..")
        {
            if (segments.empty())
            {
                return utils::result::Result<void>(
                    utils::result::ERROR, "request path escapes root by '..'");
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

    request_path_ = out;
    return utils::result::Result<void>();
}

std::string ResolvedRequestContext::extractHost_(
    const http::HttpRequest& request)
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

std::string ResolvedRequestContext::normalizeSlashes_(const std::string& path)
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
