#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{

using namespace utils::result;

Result<void> RecvRequestState::handleEvent(
    HttpSession& context, const FdEvent& event)
{
    // クライアント側のclose検出（自分のソケットのみ）
    if (event.is_opposite_close &&
        event.fd == context.context_.socket_fd.getFd())
    {
        context.context_.peer_closed = true;
        context.context_.should_close_connection = true;

        // リクエスト受信中に相手が閉じた場合、追加入力は来ない。
        // ただし user-space に残っている分だけは一度だけ処理を試みる。
        (void)context.consumeRecvBufferWithoutRead_();

        // 状態がまだRECVなら（遷移していないなら）終了
        if (context.context_.pending_state == NULL)
        {
            context.changeState(new CloseWaitState());
            context.context_.socket_fd.shutdown();
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
            if (context.context_.pending_state != NULL)
                break;

            // context_.recv_buffer が上限に達している間は read を止める。
            if (context.context_.recv_buffer.size() >=
                HttpSession::kMaxRecvBufferBytes)
            {
                (void)context.updateSocketWatches_();
                break;
            }

            // まだ足りない。ソケットから追加入力を読む。
            const ssize_t n = context.context_.recv_buffer.fillFromFd(
                context.context_.socket_fd.getFd());
            if (n < 0)
            {
                // 実装規定: read/write 後の errno 分岐は禁止。
                break;
            }
            if (n == 0)
            {
                // クライアントが切断
                context.changeState(new CloseWaitState());
                context.context_.socket_fd.shutdown();
                context.controller_.requestDelete(&context);
                return Result<void>();
            }
        }
        // context_.recv_buffer サイズに応じた read watch のON/OFF を反映する。
        (void)context.updateSocketWatches_();
    }

    return Result<void>();
}

void RecvRequestState::getWatchFlags(
    const HttpSession& session, bool* want_read, bool* want_write) const
{
    if (want_read)
    {
        *want_read = (session.context_.recv_buffer.size() <
                      HttpSession::kMaxRecvBufferBytes);
    }
    if (want_write)
    {
        *want_write = false;
    }
}

}  // namespace server
