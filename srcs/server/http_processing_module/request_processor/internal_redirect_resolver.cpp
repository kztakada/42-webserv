#include "server/http_processing_module/request_processor/internal_redirect_resolver.hpp"

namespace server
{

using utils::result::Result;

Result<http::HttpRequest> InternalRedirectResolver::buildInternalGetRequest(
    const std::string& uri_path, const http::HttpRequest& base) const
{
    http::HttpRequest req;
    std::string raw;
    raw += "GET ";
    raw += uri_path;
    raw += " HTTP/1.";
    raw += static_cast<char>('0' + base.getMinorVersion());
    raw += "\r\n";

    // Host は vserver 選択のため可能なら維持
    Result<const std::vector<std::string>&> h = base.getHeader("Host");
    if (h.isOk() && !h.unwrap().empty())
    {
        raw += "Host: ";
        raw += h.unwrap()[0];
        raw += "\r\n";
    }
    raw += "\r\n";

    std::vector<utils::Byte> buf;
    buf.reserve(raw.size());
    for (std::string::size_type i = 0; i < raw.size(); ++i)
    {
        buf.push_back(static_cast<utils::Byte>(raw[i]));
    }

    Result<size_t> parsed = req.parse(buf, NULL, false);
    if (parsed.isError())
    {
        return Result<http::HttpRequest>(
            ERROR, http::HttpRequest(), parsed.getErrorMessage());
    }
    if (!req.isParseComplete())
    {
        return Result<http::HttpRequest>(ERROR, http::HttpRequest(),
            "internal redirect request is not complete");
    }
    return req;
}

bool InternalRedirectResolver::tryBuildErrorPageInternalRedirect(
    const RequestRouter& router, const IPAddress& server_ip,
    const PortType& server_port, const http::HttpRequest& base_request,
    const http::HttpStatus& error_status, http::HttpRequest* out_next) const
{
    if (out_next == NULL)
        return false;

    Result<LocationRouting> route_result =
        router.route(base_request, server_ip, server_port);
    if (route_result.isError())
        return false;

    LocationRouting route = route_result.unwrap();
    std::string target;
    if (!route.tryGetErrorPagePath(error_status, &target))
        return false;
    if (target.empty() || target[0] != '/')
        return false;

    // 自己ループ防止（/errors/404.html の解決で同じURIに飛び続けるのを防ぐ）
    if (base_request.getPath() == target)
        return false;

    Result<http::HttpRequest> ir =
        buildInternalGetRequest(target, base_request);
    if (ir.isError())
        return false;
    *out_next = ir.unwrap();
    return true;
}

}  // namespace server
