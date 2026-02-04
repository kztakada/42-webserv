#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::handleRecvRequestReadEvent_(const FdEvent& event)
{
    (void)event;

    for (;;)
    {
        // まず、すでにバッファに溜まっている分を先に消費する（read()しない）。
        Result<void> c = consumeRecvBufferWithoutRead_();
        if (c.isError())
            return c;

        // 消費の結果として状態が変わったら（SEND/CGI等）ここで終了。
        if (state_ != RECV_REQUEST)
            break;

        // recv_buffer_ が上限に達している間は read
        // を止める（バックプレッシャー）。
        if (recv_buffer_.size() >= kMaxRecvBufferBytes)
        {
            (void)updateSocketWatches_();
            break;
        }

        // まだ足りない。ソケットから追加入力を読む。
        const ssize_t n = recv_buffer_.fillFromFd(socket_fd_.getFd());
        if (n < 0)
        {
            // 実装規定: read/write 後の errno 分岐は禁止。
            // -1 は「今は進めない」として待機し、致命エラーは reactor の
            // kErrorEvent で検出する方針にする。
            break;
        }
        if (n == 0)
        {
            // クライアントが切断
            state_ = CLOSE_WAIT;
            socket_fd_.shutdown();
            controller_.requestDelete(this);
            return Result<void>();
        }
    }

    // recv_buffer_ サイズに応じた read watch のON/OFF を反映する。
    (void)updateSocketWatches_();
    return Result<void>();
}

}  // namespace server
