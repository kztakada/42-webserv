#include "http/header.hpp"
#include "server/session/fd_session/http_session.hpp"

namespace server
{

using namespace utils::result;

Result<http::HttpRequest> HttpSession::buildInternalRedirectRequest_(
    const std::string& uri_path) const
{
    http::HttpRequest req;
    std::string raw;
    raw += "GET ";
    raw += uri_path;
    raw += " HTTP/1.";
    raw += static_cast<char>('0' + context_.request.getMinorVersion());
    raw += "\r\n";

    // Host は可能なら維持
    Result<const std::vector<std::string>&> host =
        context_.request.getHeader(http::HeaderName(http::HeaderName::HOST).toString());
    if (host.isOk() && !host.unwrap().empty())
    {
        raw += "Host: ";
        raw += host.unwrap()[0];
        raw += "\r\n";
    }
    raw += "\r\n";

    std::vector<utils::Byte> buf;
    buf.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
        buf.push_back(static_cast<utils::Byte>(raw[i]));

    Result<size_t> parsed = req.parse(buf);
    if (parsed.isError() || !req.isParseComplete())
    {
        return Result<http::HttpRequest>(ERROR, http::HttpRequest(),
            "failed to build internal redirect request");
    }
    return req;
}

}  // namespace server
