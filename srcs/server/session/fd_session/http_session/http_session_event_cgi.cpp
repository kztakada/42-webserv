#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::handleExecuteCgiEvent_(const FdEvent& event)
{
    // CGI 実行中はソケットイベントを基本的に無視し、CGI からの通知を待つ。
    if (event.fd == socket_fd_.getFd() && event.type == kReadEvent)
    {
        if (recv_buffer_.size() < kMaxRecvBufferBytes)
        {
            const ssize_t n = recv_buffer_.fillFromFd(socket_fd_.getFd());
            if (n == 0)
            {
                peer_closed_ = true;
                should_close_connection_ = true;
            }
        }
        (void)updateSocketWatches_();
    }

    if (peer_closed_)
    {
        state_ = CLOSE_WAIT;
        socket_fd_.shutdown();

        // クライアントが消えたので CGI も中断
        if (active_cgi_session_ != NULL)
        {
            controller_.requestDelete(active_cgi_session_);
            active_cgi_session_ = NULL;
        }

        controller_.requestDelete(this);
    }

    return Result<void>();
}

}  // namespace server
