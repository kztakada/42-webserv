#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{

using namespace utils::result;

Result<void> SendResponseState::handleEvent(HttpSession& context, const FdEvent& event)
{
    // クライアント切断処理
    if (event.is_opposite_close && event.fd == context.socket_fd_.getFd())
    {
         context.peer_closed_ = true;
         context.should_close_connection_ = true;
         
         // レスポンス送信中に相手が閉じた場合、送信は完遂できない。
         context.changeState(new CloseWaitState());
         context.socket_fd_.shutdown();

         if (context.active_cgi_session_ != NULL)
         {
             context.controller_.requestDelete(context.active_cgi_session_);
             context.active_cgi_session_ = NULL;
         }
         context.controller_.requestDelete(&context);
         return Result<void>();
    }

    if (event.type != kWriteEvent) return Result<void>();

    if (context.response_writer_ == NULL)
    {
        context.changeState(new CloseWaitState());
        return Result<void>(ERROR, "missing response writer");
    }

    // 送信バッファが空なら積む
    if (context.send_buffer_.size() == 0)
    {
        Result<HttpResponseWriter::PumpResult> pumped =
            context.response_writer_->pump(context.send_buffer_);
        if (pumped.isError())
        {
            context.changeState(new CloseWaitState());
            return Result<void>(ERROR, pumped.getErrorMessage());
        }

        const HttpResponseWriter::PumpResult pr = pumped.unwrap();
        if (pr.should_close_connection)
            context.should_close_connection_ = true;
    }

    // flush
    if (context.send_buffer_.size() > 0)
    {
        const ssize_t n = context.send_buffer_.flushToFd(context.socket_fd_.getFd());
        if (n < 0)
        {
            // 実装規定: read/write 後の errno 分岐は禁止。
            return Result<void>();
        }
    }

    // 送信バッファが空で、レスポンス完了なら次へ
    if (context.send_buffer_.size() == 0 && context.response_.isComplete())
    {
        // 1レスポンス完了
        if (context.response_writer_ != NULL)
        {
            delete context.response_writer_;
            context.response_writer_ = NULL;
        }
        // body_source_ は writer に渡しただけなので、ここで破棄
        if (context.body_source_ != NULL)
        {
            delete context.body_source_;
            context.body_source_ = NULL;
        }

        context.response_.reset();
        context.handler_.reset();
        context.request_ = http::HttpRequest();

        if (context.should_close_connection_)
        {
            context.changeState(new CloseWaitState());
            context.socket_fd_.shutdown();

            if (context.active_cgi_session_ != NULL)
            {
                context.controller_.requestDelete(context.active_cgi_session_);
                context.active_cgi_session_ = NULL;
            }
            context.controller_.requestDelete(&context);
            return Result<void>();
        }

        // Keep-Alive: 次のリクエストへ
        context.changeState(new RecvRequestState());
        // バッファに残っているデータを処理
        // 注意: ここで consumeRecvBufferWithoutRead_ を呼ぶと、即座に次の State に遷移する可能性がある。
        // State パターンでは遷移は呼び出し元 (handleEvent) で行われるが、
        // ここで再帰的に次のステートの処理を行うか、あるいはイベントループに戻るか。
        // consumeRecvBufferWithoutRead_ は pending_state_ をセットするだけ。
        return context.consumeRecvBufferWithoutRead_();
    }

    return Result<void>();
}

}  // namespace server
