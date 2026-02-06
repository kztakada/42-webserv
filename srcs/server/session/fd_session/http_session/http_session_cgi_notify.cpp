#include "http/cgi_response.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"

namespace server
{

using namespace utils::result;

// CgiSession からの通知: CGI stdout のヘッダが確定した
Result<void> HttpSession::onCgiHeadersReady(CgiSession& cgi)
{
    const http::CgiResponse& cr = cgi.response();
    const http::CgiResponseType t = cr.getResponseType();

    if (t == http::kLocalRedirect)
        return handleCgiHeadersReadyLocalRedirect_(cgi, cr);

    return handleCgiHeadersReadyNormal_(cgi, cr);
}

// CgiSession からの通知: CGI がヘッダ確定前に失敗した
Result<void> HttpSession::onCgiError(
    CgiSession& cgi, const std::string& message)
{
    return handleCgiError_(cgi, message);
}

}  // namespace server
