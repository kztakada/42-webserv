#ifndef WEBSERV_SESSION_CGI_HANDLER_HPP_
#define WEBSERV_SESSION_CGI_HANDLER_HPP_

#include <string>
#include "utils/result.hpp"

namespace http {
class CgiResponse;
}

namespace server {

class HttpSession;
class CgiSession;

class SessionCgiHandler {
public:
    explicit SessionCgiHandler(HttpSession& session);
    ~SessionCgiHandler();

    utils::result::Result<void> startCgi();
    utils::result::Result<void> onCgiHeadersReady(CgiSession& cgi);
    utils::result::Result<void> onCgiError(CgiSession& cgi, const std::string& message);

    CgiSession* getActiveCgiSession() const;
    void clearActiveCgiSession();

private:
    HttpSession& session_;
    CgiSession* active_cgi_session_;

    utils::result::Result<void> handleCgiHeadersReadyLocalRedirect_(CgiSession& cgi, const http::CgiResponse& cr);
    utils::result::Result<void> handleCgiHeadersReadyNormal_(CgiSession& cgi, const http::CgiResponse& cr);
    utils::result::Result<void> handleCgiError_(CgiSession& cgi, const std::string& message);
};

} // namespace server
#endif // WEBSERV_SESSION_CGI_HANDLER_HPP_
