#include "server/request_router/location_routing.hpp"

#include "utils/path.hpp"

namespace server
{

LocationRouting::LocationRouting()
    : virtual_server_(NULL),
      location_(NULL),
      request_path_(),
      host_(),
      minor_version_(1),
      status_(http::HttpStatus::SERVER_ERROR)
{
}

LocationRouting::LocationRouting(const VirtualServer* vserver,
    const LocationDirective* loc, const std::string& request_path,
    const std::string& host, int minor_version, http::HttpStatus status)
    : virtual_server_(vserver),
      location_(loc),
      request_path_(request_path),
      host_(host),
      minor_version_(minor_version),
      status_(status)
{
}

bool LocationRouting::hasVirtualServer() const
{
    return virtual_server_ != NULL;
}

bool LocationRouting::hasLocationDirective() const { return location_ != NULL; }

http::HttpStatus LocationRouting::getHttpStatus() const { return status_; }

const std::string& LocationRouting::getRequestPath() const
{
    return request_path_;
}

utils::result::Result<std::string> LocationRouting::getPath() const
{
    if (status_.isError())
    {
        return utils::result::Result<std::string>(
            utils::result::ERROR, std::string(), "routing status is error");
    }
    if (!location_)
    {
        return utils::result::Result<std::string>(
            utils::result::ERROR, std::string(), "no location matched");
    }
    return location_->getAbsolutePath(request_path_);
}

utils::result::Result<utils::path::PhysicalPath>
LocationRouting::resolvePhysicalPathUnderRootOrError(
    bool allow_nonexistent_leaf) const
{
    if (status_.isError())
    {
        return utils::result::Result<utils::path::PhysicalPath>(
            utils::result::ERROR, utils::path::PhysicalPath(),
            "routing status is error");
    }
    if (!location_)
    {
        return utils::result::Result<utils::path::PhysicalPath>(
            utils::result::ERROR, utils::path::PhysicalPath(),
            "no location matched");
    }

    std::string uri_path = location_->removePathPatternFromPath(request_path_);
    if (uri_path.empty())
    {
        uri_path = "/";
    }
    else if (uri_path[0] != '/')
    {
        uri_path = "/" + uri_path;
    }

    return utils::path::resolvePhysicalPathUnderRoot(
        location_->rootDir(), uri_path, allow_nonexistent_leaf);
}

utils::result::Result<utils::path::PhysicalPath>
LocationRouting::resolvePhysicalPathUnderRootOrError() const
{
    return resolvePhysicalPathUnderRootOrError(false);
}

utils::result::Result<std::string> LocationRouting::getErrorPagePath(
    const http::HttpStatus& status) const
{
    if (!location_)
    {
        return utils::result::Result<std::string>(utils::result::ERROR,
            std::string(), "no location for error_page lookup");
    }
    std::string target;
    if (!location_->tryGetErrorPagePath(status, &target))
    {
        return utils::result::Result<std::string>(
            utils::result::ERROR, std::string(), "no error_page configured");
    }
    if (!target.empty() && target[0] == '/')
    {
        return utils::result::Result<std::string>(utils::result::ERROR,
            std::string(), "error_page is internal redirect");
    }
    return target;
}

utils::result::Result<http::HttpRequest> LocationRouting::getErrorRequest(
    const http::HttpStatus& status) const
{
    if (!location_)
    {
        return utils::result::Result<http::HttpRequest>(utils::result::ERROR,
            http::HttpRequest(), "no location for error_page lookup");
    }
    std::string target;
    if (!location_->tryGetErrorPagePath(status, &target))
    {
        return utils::result::Result<http::HttpRequest>(utils::result::ERROR,
            http::HttpRequest(), "no error_page configured");
    }
    if (target.empty() || target[0] != '/')
    {
        return utils::result::Result<http::HttpRequest>(utils::result::ERROR,
            http::HttpRequest(), "error_page is not internal redirect");
    }

    http::HttpRequest req;
    std::string raw = buildInternalRedirectRequestRaw_(target);
    std::vector<utils::Byte> buf;
    buf.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
    {
        buf.push_back(static_cast<utils::Byte>(raw[i]));
    }
    utils::result::Result<size_t> parsed = req.parse(buf);
    if (parsed.isError() || !req.isParseComplete())
    {
        return utils::result::Result<http::HttpRequest>(utils::result::ERROR,
            http::HttpRequest(), "failed to build internal redirect request");
    }
    return req;
}

std::string LocationRouting::getDefaultErrorPage(const http::HttpStatus& status)
{
    std::string body;
    body += "<!doctype html>\n";
    body += "<html><head><meta charset=\"utf-8\">";
    body += "<title>";
    body += toString_(status);
    body += "</title></head><body>";
    body += "<h1>";
    body += toString_(status);
    body += "</h1>";
    body += "</body></html>\n";
    return body;
}

std::string LocationRouting::buildInternalRedirectRequestRaw_(
    const std::string& uri_path) const
{
    std::string req;
    req += "GET ";
    req += uri_path;
    req += " HTTP/1.";
    req += static_cast<char>('0' + minor_version_);
    req += "\r\n";
    if (!host_.empty())
    {
        req += "Host: ";
        req += host_;
        req += "\r\n";
    }
    req += "\r\n";
    return req;
}

std::string LocationRouting::toString_(const http::HttpStatus& status)
{
    std::ostringstream oss;
    oss << status.getCode() << " " << status.getMessage();
    return oss.str();
}

}  // namespace server
