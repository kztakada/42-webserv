#ifndef WEBSERV_I_HTTP_SESSION_STATE_HPP_
#define WEBSERV_I_HTTP_SESSION_STATE_HPP_

#include "server/reactor/fd_event.hpp"
#include "utils/result.hpp"

namespace server
{

class HttpSession;

class IHttpSessionState
{
   public:
    virtual ~IHttpSessionState() {}
    virtual utils::result::Result<void> handleEvent(
        HttpSession& context, const FdEvent& event) = 0;
};

}  // namespace server

#endif  // WEBSERV_I_HTTP_SESSION_STATE_HPP_
