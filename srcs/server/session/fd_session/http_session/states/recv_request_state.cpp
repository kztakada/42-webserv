#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session_controller.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

// Httpリクエストが確定したかを判断する(ログ出力用)
static bool isRequestParsingNotStarted_(const http::HttpRequest& request)
{
    if (request.getMethod() != http::HttpMethod::UNKNOWN)
        return false;
    if (!request.getMethodString().empty())
        return false;
    if (!request.getPath().empty())
        return false;
    if (!request.getHeaders().empty())
        return false;
    if (request.getDecodedBodyBytes() != 0)
        return false;
    return true;
}

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

            // バックプレッシャーが解除した場合に更新する
            if (context.context_.in_read_backpressure &&
                context.context_.recv_buffer.size() <
                    HttpSession::kMaxRecvBufferBytes)
                context.context_.in_read_backpressure = false;

            // 消費の結果として状態遷移待ちならここで終了。
            if (context.context_.pending_state != NULL)
                break;

            // context_.recv_buffer が上限に達している間は read を止める。
            if (context.context_.recv_buffer.size() >=
                HttpSession::kMaxRecvBufferBytes)
            {
                // バックプレッシャー発生時ログ出力
                if (!context.context_.in_read_backpressure)
                {
                    context.context_.in_read_backpressure = true;
                    utils::Log::warning("Read BackPressure occurred");
                }
                (void)context.updateSocketWatches_();
                break;
            }

            // まだ足りない。ソケットから追加入力を読む。
            const bool is_new_request = isRequestParsingNotStarted_(
                context.context_.request);  // ログ出力用
            const size_t before_read_buffer_size =
                context.context_.recv_buffer.size();  // ログ出力用
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

            // 新規リクエストの最初のTCP受信のみログする。
            // pipeline 等で recv_buffer に既に残りがある場合はログしない。
            if (n > 0 && is_new_request && before_read_buffer_size == 0)
            {
                const std::string request_host =
                    context.context_.socket_fd.getServerIp().toString() + ":" +
                    context.context_.socket_fd.getServerPort().toString();
                const std::string msg =
                    std::string("Host: ") + request_host +
                    " Received TCP packet top from " +
                    context.context_.socket_fd.getClientIp().toString() + ":" +
                    context.context_.socket_fd.getClientPort().toString();
                utils::Log::info(msg);
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
