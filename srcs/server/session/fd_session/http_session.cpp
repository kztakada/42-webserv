#include "server/session/fd_session/http_session.hpp"

#include "server/session/fd_session/http_session/states/http_session_states.hpp"

namespace server
{

using namespace utils::result;

HttpSession::HttpSession(int fd, const SocketAddress& server_addr,
    const SocketAddress& client_addr, FdSessionController& controller,
    HttpProcessingModule& module)
    : FdSession(controller, kDefaultTimeoutSec),
      context_(fd, server_addr, client_addr, controller, module.router),
      module_(module)
{
    context_.current_state = new RecvRequestState();
    updateLastActiveTime();
}

HttpSession::~HttpSession()
{
    // cleanup is handled by SessionContext destructor
}

void HttpSession::getInitialWatchSpecs(
    std::vector<FdSession::FdWatchSpec>* out) const
{
    if (out == NULL)
        return;
    // 最初は read のみ watch し、書き込みは必要時だけ有効化する。
    out->push_back(
        FdSession::FdWatchSpec(context_.socket_fd.getFd(), true, false));
}

bool HttpSession::isComplete() const { return context_.is_complete; }

void HttpSession::changeState(IHttpSessionState* next_state)
{
    if (context_.pending_state != NULL)
    {
        delete context_.pending_state;
    }
    context_.pending_state = next_state;
}

Result<void> HttpSession::onCgiHeadersReady(CgiSession& cgi)
{
    return module_.cgi_handler.onCgiHeadersReady(*this, cgi);
}

Result<void> HttpSession::onCgiError(
    CgiSession& cgi, const std::string& message)
{
    return module_.cgi_handler.onCgiError(*this, cgi, message);
}

}  // namespace server
