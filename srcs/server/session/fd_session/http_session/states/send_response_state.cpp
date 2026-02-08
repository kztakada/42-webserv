#include <sstream>
#include <string>
#include <vector>

#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session_controller.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

Result<void> SendResponseState::handleEvent(
    HttpSession& context, const FdEvent& event)
{
    // クライアント切断処理
    if (event.is_opposite_close &&
        event.fd == context.context_.socket_fd.getFd())
    {
        context.context_.peer_closed = true;
        context.context_.should_close_connection = true;

        // レスポンス送信中に相手が閉じた場合、送信は完遂できない。
        context.changeState(new CloseWaitState());
        context.context_.socket_fd.shutdown();

        if (context.getContext().active_cgi_session != NULL)
        {
            context.controller_.requestDelete(
                context.getContext().active_cgi_session);
            context.getContext().active_cgi_session = NULL;
        }
        context.controller_.requestDelete(&context);
        return Result<void>();
    }

    if (event.type != kWriteEvent)
        return Result<void>();

    if (context.context_.response_writer == NULL)
    {
        context.changeState(new CloseWaitState());
        return Result<void>(ERROR, "missing response writer");
    }

    // 送信バッファが空なら積む
    if (context.context_.send_buffer.size() == 0)
    {
        Result<HttpResponseWriter::PumpResult> pumped =
            context.context_.response_writer->pump(
                context.context_.send_buffer);
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
        const ssize_t n = context.context_.send_buffer.flushToFd(
            context.context_.socket_fd.getFd());
        if (n < 0)
        {
            if (context.processingLog() != NULL)
                context.processingLog()->incrementBlockIo();  // ログ計測
            // バックプレッシャー発生時ログ出力
            if (!context.context_.in_write_backpressure)
            {
                context.context_.in_write_backpressure = true;
                utils::Log::warning("Write BackPressure occurred");
            }
            // 実装規定: read/write 後の errno 分岐は禁止。
            return Result<void>();
        }
        // バックプレッシャーログ出力後、解除する
        if (n > 0 && context.context_.in_write_backpressure)
            context.context_.in_write_backpressure = false;
    }

    if (context.context_.send_buffer.size() == 0 &&
        context.context_.in_write_backpressure)
        context.context_.in_write_backpressure = false;

    // 送信バッファが空で、レスポンス完了なら次へ
    if (context.context_.send_buffer.size() == 0 &&
        context.context_.response.isComplete())
    {
        // リクエスト処理時間（最初の recv 〜 send 完了）を記録　ログ計測
        if (context.processingLog() != NULL &&
            context.context_.has_request_start_time)
        {
            const long end_seconds = utils::Timestamp::nowEpochSeconds();
            long elapsed =
                end_seconds - context.context_.request_start_time_seconds;
            if (elapsed < 0)
                elapsed = 0;
            context.processingLog()->recordRequestTimeSeconds(elapsed);
        }
        context.context_.has_request_start_time = false;
        context.context_.request_start_time_seconds = 0;

        // レスポンス送信完了ログ（1回だけ）
        std::string request_host =
            context.context_.socket_fd.getServerIp().toString() + ":" +
            context.context_.socket_fd.getServerPort().toString();
        Result<const std::vector<std::string>&> host_header =
            context.context_.request.getHeader("Host");
        if (host_header.isOk())
        {
            const std::vector<std::string>& values = host_header.unwrap();
            if (!values.empty() && !values[0].empty() &&
                request_host != values[0])
                request_host = request_host + "(" + values[0] + ")";
        }

        const http::HttpStatus status = context.context_.response.getStatus();
        std::string reason = context.context_.response.getReasonPhrase();
        if (reason.empty())
            reason = status.getMessage();

        std::ostringstream oss;
        oss << "Host: " << request_host << " Sent response " << status.getCode()
            << " " << reason << " to "
            << context.context_.socket_fd.getClientIp().toString() << ":"
            << context.context_.socket_fd.getClientPort().toString();
        utils::Log::info(oss.str());

        // 1レスポンス完了
        if (context.context_.response_writer != NULL)
        {
            delete context.context_.response_writer;
            context.context_.response_writer = NULL;
        }
        // context_.body_source は writer に渡しただけなので、ここで破棄
        context.context_.body_source.reset(NULL);

        context.context_.response.reset();
        context.getContext().request_handler.reset();
        context.context_.request = http::HttpRequest();

        if (context.context_.should_close_connection)
        {
            context.changeState(new CloseWaitState());
            context.context_.socket_fd.shutdown();

            if (context.getContext().active_cgi_session != NULL)
            {
                context.controller_.requestDelete(
                    context.getContext().active_cgi_session);
                context.getContext().active_cgi_session = NULL;
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

void SendResponseState::getWatchFlags(
    const HttpSession& session, bool* want_read, bool* want_write) const
{
    (void)session;
    if (want_read)
    {
        *want_read = false;
    }
    if (want_write)
    {
        *want_write = true;
    }
}

}  // namespace server