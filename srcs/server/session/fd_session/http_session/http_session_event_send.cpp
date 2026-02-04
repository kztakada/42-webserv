#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::handleSendResponseWriteEvent_(const FdEvent& event)
{
    (void)event;

    if (response_writer_ == NULL)
    {
        state_ = CLOSE_WAIT;
        return Result<void>(ERROR, "missing response writer");
    }

    // 送信バッファが空なら積む
    if (send_buffer_.size() == 0)
    {
        Result<HttpResponseWriter::PumpResult> pumped =
            response_writer_->pump(send_buffer_);
        if (pumped.isError())
        {
            state_ = CLOSE_WAIT;
            return Result<void>(ERROR, pumped.getErrorMessage());
        }

        const HttpResponseWriter::PumpResult pr = pumped.unwrap();
        if (pr.should_close_connection)
            should_close_connection_ = true;

        // ここでDONEでも、flushするまで完了しない
    }

    // flush
    if (send_buffer_.size() > 0)
    {
        const ssize_t n = send_buffer_.flushToFd(socket_fd_.getFd());
        if (n < 0)
        {
            // 実装規定: read/write 後の errno 分岐は禁止。
            // -1 は「今は送れない」として待機。
            return Result<void>();
        }
    }

    // 送信バッファが空で、レスポンス完了なら次へ
    if (send_buffer_.size() == 0 && response_.isComplete())
    {
        // 1レスポンス完了
        if (response_writer_ != NULL)
        {
            delete response_writer_;
            response_writer_ = NULL;
        }
        // body_source_ は writer に渡しただけなので、ここで破棄
        if (body_source_ != NULL)
        {
            delete body_source_;
            body_source_ = NULL;
        }

        response_.reset();
        handler_.reset();
        request_ = http::HttpRequest();

        if (should_close_connection_)
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

        // keep-alive 継続: 状態をリセット
        peer_closed_ = false;
        should_close_connection_ = false;

        // active_cgi_session_ は controller 所有。ここでは delete しない。

        // keep-alive: 次のリクエストへ
        state_ = RECV_REQUEST;
        (void)updateSocketWatches_();

        // pipelining 等で user-space に次リクエストが残っている場合は、
        // イベント無しでも進められる分だけ処理する。
        (void)consumeRecvBufferWithoutRead_();
    }

    return Result<void>();
}

}  // namespace server
