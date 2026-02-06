#ifndef WEBSERV_SESSION_CGI_HANDLER_HPP_
#define WEBSERV_SESSION_CGI_HANDLER_HPP_

#include <string>

#include "utils/result.hpp"

namespace http
{
class CgiResponse;
}

namespace server
{

class HttpSession;
class CgiSession;
class FdSessionController;

class SessionCgiHandler
{
   public:
    explicit SessionCgiHandler(FdSessionController& controller);
    ~SessionCgiHandler();

    utils::result::Result<void> startCgi(HttpSession& session);
    utils::result::Result<void> onCgiHeadersReady(
        HttpSession& session, CgiSession& cgi);
    utils::result::Result<void> onCgiError(
        HttpSession& session, CgiSession& cgi, const std::string& message);

   private:
    FdSessionController& controller_;

    utils::result::Result<void> handleCgiHeadersReadyLocalRedirect_(
        HttpSession& session, CgiSession& cgi, const http::CgiResponse& cr);
    utils::result::Result<void> handleCgiHeadersReadyNormal_(
        HttpSession& session, CgiSession& cgi, const http::CgiResponse& cr);
    utils::result::Result<void> handleCgiError_(
        HttpSession& session, CgiSession& cgi, const std::string& message);
};

}  // namespace server
#endif  // WEBSERV_SESSION_CGI_HANDLER_HPP_
