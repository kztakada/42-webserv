#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::handleEvent(const FdEvent& event)
{
    updateLastActiveTime();

    if (state_ == CLOSE_WAIT)
        return Result<void>();

    if (event.type == kTimeoutEvent)
    {
        state_ = CLOSE_WAIT;
        socket_fd_.shutdown();
        controller_.requestDelete(this);
        return Result<void>();
    }

    if (event.type == kErrorEvent)
    {
        state_ = CLOSE_WAIT;
        socket_fd_.shutdown();
        controller_.requestDelete(this);
        return Result<void>();
    }

    if (event.is_opposite_close)
    {
        // クライアント側のclose検出（自分のソケットのみ）
        if (event.fd == socket_fd_.getFd())
        {
            peer_closed_ = true;
            should_close_connection_ = true;

            // リクエスト受信中に相手が閉じた場合、追加入力は来ない。
            // ただし user-space に残っている分だけは一度だけ処理を試みる。
            if (state_ == RECV_REQUEST)
            {
                (void)consumeRecvBufferWithoutRead_();
                if (state_ == RECV_REQUEST)
                {
                    state_ = CLOSE_WAIT;
                    socket_fd_.shutdown();
                    controller_.requestDelete(this);
                    return Result<void>();
                }
            }

            // レスポンス送信中に相手が閉じた場合、送信は完遂できない。
            // write ready が来ないまま停止するのを防ぐため即終了する。
            if (state_ == SEND_RESPONSE)
            {
                state_ = CLOSE_WAIT;
                socket_fd_.shutdown();

                if (active_cgi_session_ != NULL)
                {
                    controller_.requestDelete(active_cgi_session_);
                    active_cgi_session_ = NULL;
                }
                controller_.requestDelete(this);
                return Result<void>();
            }
        }
    }

    if (state_ == EXECUTE_CGI)
    {
        return handleExecuteCgiEvent_(event);
    }

    if (state_ == RECV_REQUEST && event.type == kReadEvent)
    {
        return handleRecvRequestReadEvent_(event);
    }

    if (state_ == SEND_RESPONSE && event.type == kWriteEvent)
    {
        return handleSendResponseWriteEvent_(event);
    }

    return Result<void>();
}

}  // namespace server
