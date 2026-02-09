#include "server/session/fd_session/http_session.hpp"

#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

HttpSession::HttpSession(int fd, const SocketAddress& server_addr,
    const SocketAddress& client_addr, FdSessionController& controller,
    HttpProcessingModule& module, utils::ProcessingLog* processing_log)
    : FdSession(controller, kDefaultTimeoutSec),
      context_(fd, server_addr, client_addr, module.router),
      module_(module),
      processing_log_(processing_log),
      is_counted_as_active_connection_(false)
{
    context_.current_state = new RecvRequestState();
    updateLastActiveTime();
}

HttpSession::~HttpSession()
{
    if (processing_log_ != NULL && is_counted_as_active_connection_)
        processing_log_->onConnectionClosed();  // ログ計測
    clearBodyWatch_();
    // cleanup is handled by SessionContext destructor
}

// ログ計測
void HttpSession::markCountedAsActiveConnection()
{
    if (processing_log_ == NULL)
        return;
    if (is_counted_as_active_connection_)
        return;
    is_counted_as_active_connection_ = true;
    processing_log_->onConnectionOpened();
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

bool HttpSession::isTimedOut() const
{
    if (timeout_seconds_ <= 0)
        return false;

    // CGI 実行中は、HttpSession のタイムアウトで CloseWait に落とすと
    // CGI timeout 由来の 504/502 を返せず、クライアントが無応答で待ち続ける。
    if (dynamic_cast<const ExecuteCgiState*>(context_.current_state) != NULL)
        return false;

    // headers は確定したが body が来るまでヘッダ送出を止めている間も、
    // CGI 側の timeout で 504 に差し替えるため HttpSession は timeout
    // させない。
    if (dynamic_cast<const SendResponseState*>(context_.current_state) !=
            NULL &&
        context_.pause_write_until_body_ready)
        return false;

    return FdSession::isTimedOut();
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
