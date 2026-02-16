#include <ctime>
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

Result<void> SendResponseState::switchToInternalServerErrorAndClose_(
    HttpSession& session, const std::string& message) const
{
    utils::Log::error(
        "SendResponseState", "response body read error:", message);

    session.cleanupCgiOnClose_();
    session.clearBodyWatch_();
    session.context_.pause_write_until_body_ready = false;

    // ヘッダが既に送出済み（例: chunked の途中）で 500 に差し替えると、
    // 送信中のレスポンスの後ろに別の HTTP レスポンスを混ぜてしまい、
    // クライアント側で chunked フォーマットが壊れる。
    // この場合は 500 を送らず、可能なら EOF(終端)だけ送って接続を閉じる。
    if (session.context_.response.phase() !=
        http::HttpResponse::kWaitingForHeaders)
    {
        session.context_.should_close_connection = true;

        if (session.context_.response_writer != NULL &&
            !session.context_.response.isComplete())
        {
            Result<void> we = session.context_.response_writer->writeEof(
                session.context_.send_buffer);
            if (we.isError())
            {
                session.changeState(new CloseWaitState());
                session.context_.socket_fd.shutdown();
                session.controller_.requestDelete(&session);
                return Result<void>();
            }
        }

        (void)session.updateSocketWatches_();
        return Result<void>();
    }

    // 送信途中のデータは破棄し、500 に差し替える。
    session.context_.send_buffer.consume(session.context_.send_buffer.size());

    RequestProcessor::Output out;
    Result<void> bo =
        session.buildErrorOutput_(http::HttpStatus::SERVER_ERROR, &out);
    if (bo.isError())
    {
        session.changeState(new CloseWaitState());
        (void)session.updateSocketWatches_();
        return bo;
    }

    session.context_.response.setHttpVersion(
        session.context_.request.getHttpVersion());
    (void)session.context_.response.setHeader("Connection", "close");
    session.context_.should_close_connection = true;

    session.installBodySourceAndWriter_(out.body_source);
    session.changeState(new SendResponseState());
    (void)session.updateSocketWatches_();
    return Result<void>();
}

using namespace utils::result;

Result<void> SendResponseState::handleEvent(
    HttpSession& context, const FdEvent& event)
{
    // CGI stdout (body fd) の read イベント: body が来たら pump して socket
    // へ流す
    if (event.type == kReadEvent && context.context_.body_watch_fd >= 0 &&
        event.fd == context.context_.body_watch_fd)
    {
        // headers 完了後、最初の body を待っている場合はここで送信を開始する
        if (context.context_.response_writer == NULL &&
            context.context_.cgi_stdout_fd_for_response == event.fd)
        {
            // 「待ち」状態を解除し、CGI レスポンスの送信を開始する
            BodySource* bs = new PrefetchedFdBodySource(
                context.context_.cgi_stdout_fd_for_response,
                context.context_.cgi_prefetched_body);
            context.installBodySourceAndWriter_(
                utils::OwnedPtr<BodySource>(bs));
            context.context_.cgi_stdout_fd_for_response = -1;
            context.context_.cgi_prefetched_body.clear();
            context.context_.waiting_cgi_first_body = false;
            context.context_.waiting_cgi_first_body_start = 0;
        }

        context.context_.pause_write_until_body_ready = false;

        // send_buffer が空なら pump して積む
        if (context.context_.send_buffer.size() == 0 &&
            context.context_.response_writer != NULL &&
            !context.context_.response.isComplete())
        {
            Result<HttpResponseWriter::PumpResult> pumped =
                context.context_.response_writer->pump(
                    context.context_.send_buffer);
            if (pumped.isError())
                return switchToInternalServerErrorAndClose_(
                    context, pumped.getErrorMessage());
            // pump の結果で send_buffer が空のままなら write を止める
            if (context.context_.send_buffer.size() == 0 &&
                !context.context_.response.isComplete())
            {
                context.context_.pause_write_until_body_ready = true;
            }
        }

        (void)context.updateSocketWatches_();
        return Result<void>();
    }

    // クライアント切断処理
    if (event.is_opposite_close &&
        event.fd == context.context_.socket_fd.getFd())
    {
        // peer が write 側を閉じた（ハーフクローズ）。
        // この場合でも peer はレスポンスを read できるため、送信は継続する。
        // ただし、次のリクエストは来ないので keep-alive は維持しない。
        context.context_.peer_closed = true;
        context.context_.should_close_connection = true;
    }

    if (event.type != kWriteEvent)
        return Result<void>();

    // body ready を待っている間は write を処理しない
    if (context.context_.pause_write_until_body_ready)
        return Result<void>();

    if (context.context_.response_writer == NULL)
    {
        context.changeState(new CloseWaitState());
        return Result<void>(ERROR, "missing response writer");
    }

    // 送信バッファが空なら積む
    if (context.context_.send_buffer.size() == 0)
    {
        // body fd を watch している（典型: CGI stdout）場合、write イベントで
        // body を read しに行くと「まだ body が来ていない」タイミングで
        // read(-1) となり、送信途中に 500 を差し込む原因になる。 send_buffer
        // が空で response が未完了なら、body の read イベントを待つ。
        if (context.context_.body_watch_fd >= 0 &&
            !context.context_.response.isComplete())
        {
            context.context_.pause_write_until_body_ready = true;
            (void)context.updateSocketWatches_();
            return Result<void>();
        }

        Result<HttpResponseWriter::PumpResult> pumped =
            context.context_.response_writer->pump(
                context.context_.send_buffer);
        if (pumped.isError())
            return switchToInternalServerErrorAndClose_(
                context, pumped.getErrorMessage());

        const HttpResponseWriter::PumpResult pr = pumped.unwrap();
        if (pr.should_close_connection)
            context.context_.should_close_connection = true;

        // pump しても何も積めない場合（典型: CGI body が would-block）は
        // write watch を止め、body fd の read を待つ。
        if (context.context_.send_buffer.size() == 0 &&
            !context.context_.response.isComplete())
        {
            context.context_.pause_write_until_body_ready = true;
            (void)context.updateSocketWatches_();
            return Result<void>();
        }
    }

    // flush
    if (context.context_.send_buffer.size() > 0)
    {
        const ssize_t n = context.context_.send_buffer.flushToFd(
            context.context_.socket_fd.getFd());
        if (n < 0)
        {
            context.changeState(new CloseWaitState());
            return Result<void>(ERROR, "event fd write failed");
        }
        if (n == 0)
        {
            // ノンブロッキング接続に対してはスルー
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

        // CGI が紐づいていればここで確実に回収する（zombie/リーク防止）
        if (context.getContext().active_cgi_session != NULL)
        {
            context.controller_.requestDelete(
                context.getContext().active_cgi_session);
            context.getContext().active_cgi_session = NULL;
        }
        // context_.body_source は writer に渡しただけなので、ここで破棄
        context.clearBodyWatch_();
        context.context_.body_source.reset(NULL);

        context.context_.response.reset();
        context.getContext().request_handler.reset();
        context.context_.request = http::HttpRequest();
        context.context_.pause_write_until_body_ready = false;

        if (context.context_.should_close_connection)
        {
            context.changeState(new CloseWaitState());
            context.context_.socket_fd.shutdown();

            context.cleanupCgiOnClose_();
            context.controller_.requestDelete(&context);
            return Result<void>();
        }

        // Keep-Alive: 次のリクエストへ
        context.changeState(new RecvRequestState());
        // pending_state を参照して watch 仕様が決まるので、ここで read watch
        // を復帰させる。
        (void)context.updateSocketWatches_();
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
        if (session.context_.pause_write_until_body_ready)
        {
            *want_write = false;
        }
        else
        {
            // send_buffer が空でも、ヘッダ送出/処理を進めるために write
            // を有効化する。
            *want_write = (session.context_.response_writer != NULL);
        }
    }
}

}  // namespace server
