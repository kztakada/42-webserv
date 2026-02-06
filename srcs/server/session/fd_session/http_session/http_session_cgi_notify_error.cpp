#include <unistd.h>

#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::handleCgiError_(
    CgiSession& cgi, const std::string& message)
{
    if (pending_state_ != NULL) return Result<void>();

    if (dynamic_cast<CloseWaitState*>(current_state_))
        return Result<void>();

    // CGI 実行中以外に通知が来た場合は安全に無視する。
    if (!dynamic_cast<ExecuteCgiState*>(current_state_))
        return Result<void>();

    utils::Log::error("CGI error: ", message);

    // CGI の stdout を閉じて詰まり/イベントループを防ぐ。
    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd >= 0)
        ::close(stdout_fd);

    // CgiSession は controller が所有。ここで削除要求だけ出す。
    if (active_cgi_session_ == &cgi)
    {
        controller_.requestDelete(active_cgi_session_);
        active_cgi_session_ = NULL;
    }
    else
    {
        controller_.requestDelete(&cgi);
    }

    response_.reset();
    response_.setHttpVersion(request_.getHttpVersion());

    RequestProcessor::Output out;
    http::HttpStatus st = http::HttpStatus::BAD_GATEWAY;
    if (message.find("timeout") != std::string::npos)
    {
        st = http::HttpStatus::GATEWAY_TIMEOUT;
    }
    Result<void> bo = buildErrorOutput_(st, &out);
    if (bo.isError())
    {
        out.body_source = NULL;
        out.should_close_connection = false;
    }
    response_.setHttpVersion(request_.getHttpVersion());

    installBodySourceAndWriter_(out.body_source);

    should_close_connection_ = should_close_connection_ || peer_closed_ ||
                               out.should_close_connection ||
                               !request_.shouldKeepAlive() ||
                               handler_.shouldCloseConnection();
    changeState(new SendResponseState());
    (void)updateSocketWatches_();
    return Result<void>();
}

}  // namespace server