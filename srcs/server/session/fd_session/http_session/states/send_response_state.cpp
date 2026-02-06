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
    if (event.is_opposite_close && event.fd == context.context_.socket_fd.getFd())
    {
         context.context_.peer_closed = true;
         context.context_.should_close_connection = true;
         
         // レスポンス送信中に相手が閉じた場合、送信は完遂できない。
         context.changeState(new CloseWaitState());
         context.context_.socket_fd.shutdown();

         if (context.cgi_handler_.getActiveCgiSession() != NULL)
         {
             context.controller_.requestDelete(context.cgi_handler_.getActiveCgiSession());
             context.cgi_handler_.clearActiveCgiSession();
         }
         context.controller_.requestDelete(&context);
         return Result<void>();
    }

    if (event.type != kWriteEvent) return Result<void>();

    if (context.context_.response_writer == NULL)
    {
        context.changeState(new CloseWaitState());
        return Result<void>(ERROR, "missing response writer");
    }

    // 送信バッファが空なら積む
    if (context.context_.send_buffer.size() == 0)
    {
        Result<HttpResponseWriter::PumpResult> pumped =
            context.context_.response_writer->pump(context.context_.send_buffer);
        if (pumped.isError())
        {
            context.changeState(new CloseWaitState());
            return Result<void>(ERROR, pumped.getErrorMessage());
        }

        const HttpResponseWriter::PumpResult pr = pumped.unwrap();
        if (pr.should_close_connection)
            context.context_.should_close_connection = true;
    }

    // flush
    if (context.context_.send_buffer.size() > 0)
    {
        const ssize_t n = context.context_.send_buffer.flushToFd(context.context_.socket_fd.getFd());
        if (n < 0)
        {
            // 実装規定: read/write 後の errno 分岐は禁止。
            return Result<void>();
        }
    }

    // 送信バッファが空で、レスポンス完了なら次へ
    if (context.context_.send_buffer.size() == 0 && context.context_.response.isComplete())
    {
        // 1レスポンス完了
        if (context.context_.response_writer != NULL)
        {
            delete context.context_.response_writer;
            context.context_.response_writer = NULL;
        }
        // context_.body_source は writer に渡しただけなので、ここで破棄
        if (context.context_.body_source != NULL)
        {
            delete context.context_.body_source;
            context.context_.body_source = NULL;
        }

        context.context_.response.reset();
        context.dispatcher_.handler().reset();
        context.context_.request = http::HttpRequest();

        if (context.context_.should_close_connection)
        {
            context.changeState(new CloseWaitState());
            context.context_.socket_fd.shutdown();

            if (context.cgi_handler_.getActiveCgiSession() != NULL)
            {
                context.controller_.requestDelete(context.cgi_handler_.getActiveCgiSession());
                context.cgi_handler_.clearActiveCgiSession();
            }
            context.controller_.requestDelete(&context);
            return Result<void>();
        }

        // Keep-Alive: 次のリクエストへ
        context.changeState(new RecvRequestState());
        // バッファに残っているデータを処理
        return context.consumeRecvBufferWithoutRead_();
    }

    return Result<void>();
}

}  // namespace server