#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::updateSocketWatches_()
{
    const int fd = context_.socket_fd.getFd();
    if (fd < 0)
        return Result<void>();

    bool want_read = false;
    bool want_write = false;

    IHttpSessionState* s = context_.pending_state ? context_.pending_state : context_.current_state;

    // dynamic_castで状態判定
    if (dynamic_cast<RecvRequestState*>(s))
    {
        want_read = (context_.recv_buffer.size() < kMaxRecvBufferBytes);
    }
    else if (dynamic_cast<SendResponseState*>(s))
    {
        want_write = true;
    }
    else if (dynamic_cast<ExecuteCgiState*>(s))
    {
        // CGI 実行中も client close 検出のため read を見る。
        // パイプラインの次リクエストは context_.recv_buffer に溜める（上限あり）。
        want_read = (context_.recv_buffer.size() < kMaxRecvBufferBytes);
    }
    else if (dynamic_cast<CloseWaitState*>(s))
    {
        // CLOSE_WAIT: ソケットwatchなし。
    }

    if (want_read == context_.socket_watch_read && want_write == context_.socket_watch_write)
        return Result<void>();

    context_.socket_watch_read = want_read;
    context_.socket_watch_write = want_write;

    Result<void> u = controller_.updateWatch(fd, want_read, want_write);
    if (u.isError())
        return Result<void>(ERROR, u.getErrorMessage());
    return Result<void>();
}

}  // namespace server