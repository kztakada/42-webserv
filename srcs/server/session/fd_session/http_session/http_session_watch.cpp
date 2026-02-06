#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::updateSocketWatches_()
{
    const int fd = socket_fd_.getFd();
    if (fd < 0)
        return Result<void>();

    bool want_read = false;
    bool want_write = false;

    IHttpSessionState* s = pending_state_ ? pending_state_ : current_state_;

    // dynamic_castで状態判定
    if (dynamic_cast<RecvRequestState*>(s))
    {
        want_read = (recv_buffer_.size() < kMaxRecvBufferBytes);
    }
    else if (dynamic_cast<SendResponseState*>(s))
    {
        want_write = true;
    }
    else if (dynamic_cast<ExecuteCgiState*>(s))
    {
        // CGI 実行中も client close 検出のため read を見る。
        // パイプラインの次リクエストは recv_buffer_ に溜める（上限あり）。
        want_read = (recv_buffer_.size() < kMaxRecvBufferBytes);
    }
    else if (dynamic_cast<CloseWaitState*>(s))
    {
        // CLOSE_WAIT: ソケットwatchなし。
    }

    if (want_read == socket_watch_read_ && want_write == socket_watch_write_)
        return Result<void>();

    socket_watch_read_ = want_read;
    socket_watch_write_ = want_write;

    Result<void> u = controller_.updateWatch(fd, want_read, want_write);
    if (u.isError())
        return Result<void>(ERROR, u.getErrorMessage());
    return Result<void>();
}

}  // namespace server