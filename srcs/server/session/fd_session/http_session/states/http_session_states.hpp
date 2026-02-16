#ifndef WEBSERV_HTTP_SESSION_STATES_HPP_
#define WEBSERV_HTTP_SESSION_STATES_HPP_

#include <string>

#include "server/session/fd_session/http_session/states/i_http_session_state.hpp"

namespace server
{

class RecvRequestState : public IHttpSessionState
{
   public:
    virtual utils::result::Result<void> handleEvent(
        HttpSession& context, const FdEvent& event);
    virtual void getWatchFlags(
        const HttpSession& session, bool* want_read, bool* want_write) const;
};

class ExecuteCgiState : public IHttpSessionState
{
   public:
    virtual utils::result::Result<void> handleEvent(
        HttpSession& context, const FdEvent& event);
    virtual void getWatchFlags(
        const HttpSession& session, bool* want_read, bool* want_write) const;
};

class SendResponseState : public IHttpSessionState
{
   public:
    virtual utils::result::Result<void> handleEvent(
        HttpSession& context, const FdEvent& event);
    virtual void getWatchFlags(
        const HttpSession& session, bool* want_read, bool* want_write) const;

   private:
    utils::result::Result<void> switchToInternalServerErrorAndClose_(
        HttpSession& session, const std::string& message) const;
};

class CloseWaitState : public IHttpSessionState
{
   public:
    virtual utils::result::Result<void> handleEvent(
        HttpSession& context, const FdEvent& event);
    virtual void getWatchFlags(
        const HttpSession& session, bool* want_read, bool* want_write) const;
};

}  // namespace server

#endif  // WEBSERV_HTTP_SESSION_STATES_HPP_
