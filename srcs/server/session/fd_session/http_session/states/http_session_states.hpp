#ifndef WEBSERV_HTTP_SESSION_STATES_HPP_
#define WEBSERV_HTTP_SESSION_STATES_HPP_

#include "server/session/fd_session/http_session/states/i_http_session_state.hpp"

namespace server
{

class RecvRequestState : public IHttpSessionState
{
   public:
    virtual utils::result::Result<void> handleEvent(
        HttpSession& context, const FdEvent& event);
};

class ExecuteCgiState : public IHttpSessionState
{
   public:
    virtual utils::result::Result<void> handleEvent(
        HttpSession& context, const FdEvent& event);
};

class SendResponseState : public IHttpSessionState
{
   public:
    virtual utils::result::Result<void> handleEvent(
        HttpSession& context, const FdEvent& event);
};

class CloseWaitState : public IHttpSessionState
{
   public:
    virtual utils::result::Result<void> handleEvent(
        HttpSession& context, const FdEvent& event);
};

}  // namespace server

#endif  // WEBSERV_HTTP_SESSION_STATES_HPP_
