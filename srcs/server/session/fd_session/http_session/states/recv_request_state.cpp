#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{

using namespace utils::result;

Result<void> RecvRequestState::handleEvent(HttpSession& context, const FdEvent& event)
{
    // クライアント側のclose検出（自分のソケットのみ）
    if (event.is_opposite_close && event.fd == context.socket_fd_.getFd())
    {
        context.peer_closed_ = true;
        context.should_close_connection_ = true;

        // リクエスト受信中に相手が閉じた場合、追加入力は来ない。
        // ただし user-space に残っている分だけは一度だけ処理を試みる。
        (void)context.consumeRecvBufferWithoutRead_();
        
        // 状態がまだRECVなら（遷移していないなら）終了
        if (context.pending_state_ == NULL)
        {
            context.changeState(new CloseWaitState());
            context.socket_fd_.shutdown();
            context.controller_.requestDelete(&context);
            return Result<void>();
        }
        return Result<void>();
    }

    if (event.type == kReadEvent)
    {
        for (;;)
        {
            // まず、すでにバッファに溜まっている分を先に消費する（read()しない）。
            Result<void> c = context.consumeRecvBufferWithoutRead_();
            if (c.isError())
                return c;

            // 消費の結果として状態遷移待ちならここで終了。
            if (context.pending_state_ != NULL)
                break;

            // recv_buffer_ が上限に達している間は read を止める。
            if (context.recv_buffer_.size() >= HttpSession::kMaxRecvBufferBytes)
            {
                (void)context.updateSocketWatches_();
                break;
            }

            // まだ足りない。ソケットから追加入力を読む。
            const ssize_t n = context.recv_buffer_.fillFromFd(context.socket_fd_.getFd());
            if (n < 0)
            {
                // 実装規定: read/write 後の errno 分岐は禁止。
                break;
            }
            if (n == 0)
            {
                // クライアントが切断
                context.changeState(new CloseWaitState());
                context.socket_fd_.shutdown();
                context.controller_.requestDelete(&context);
                return Result<void>();
            }
        }
        // recv_buffer_ サイズに応じた read watch のON/OFF を反映する。
        (void)context.updateSocketWatches_();
    }

    return Result<void>();
}

}  // namespace server
