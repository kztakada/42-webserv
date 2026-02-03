#include "server/session/fd_session/http_session.hpp"

#include <unistd.h>

#include <map>
#include <sstream>

#include "http/cgi_meta_variables.hpp"
#include "http/header.hpp"
#include "server/session/fd/cgi_pipe/cgi_pipe_fd.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

static http::HttpResponseEncoder::Options makeEncoderOptions(
    const http::HttpRequest& request)
{
    http::HttpResponseEncoder::Options opt;
    opt.request_minor_version = request.getMinorVersion();
    opt.request_should_keep_alive = request.shouldKeepAlive();
    opt.request_is_head = (request.getMethod() == http::HttpMethod::HEAD);
    return opt;
}

HttpSession::~HttpSession()
{
    if (response_writer_ != NULL)
    {
        delete response_writer_;
        response_writer_ = NULL;
    }
    if (body_source_ != NULL)
    {
        delete body_source_;
        body_source_ = NULL;
    }
}

bool HttpSession::isComplete() const { return is_complete_; }

void HttpSession::getInitialWatchSpecs(
    std::vector<FdSession::FdWatchSpec>* out) const
{
    if (out == NULL)
        return;
    // 最初は read のみ watch し、書き込みは必要時だけ有効化する。
    out->push_back(FdSession::FdWatchSpec(socket_fd_.getFd(), true, false));
}

bool HttpSession::isWaitingForCgi() const { return waiting_for_cgi_; }

void HttpSession::setWaitingForCgi(bool waiting) { waiting_for_cgi_ = waiting; }

static Result<void> setSimpleErrorResponse(
    http::HttpResponse& response, http::HttpStatus status)
{
    response.reset();
    Result<void> s = response.setStatus(status);
    if (s.isError())
        return s;

    // ボディは別レイヤで作る想定。ここではヘッダだけ。
    response.removeHeader(
        http::HeaderName(http::HeaderName::TRANSFER_ENCODING).toString());
    response.removeHeader(
        http::HeaderName(http::HeaderName::CONTENT_LENGTH).toString());
    return Result<void>();
}

Result<void> HttpSession::updateSocketWatches_()
{
    const int fd = socket_fd_.getFd();
    if (fd < 0)
        return Result<void>();

    bool want_read = false;
    bool want_write = false;

    if (!is_complete_)
    {
        if (state_ == RECV_REQUEST)
        {
            want_read = (recv_buffer_.size() < kMaxRecvBufferBytes);
        }
        else if (state_ == SEND_RESPONSE)
        {
            want_write = true;
        }
        else if (state_ == EXECUTE_CGI)
        {
            // CGI 実行中も client close 検出のため read を見る。
            // パイプラインの次リクエストは recv_buffer_ に溜める（上限あり）。
            want_read = (recv_buffer_.size() < kMaxRecvBufferBytes);
        }
        else
        {
            // CLOSE_WAIT: ソケットwatchなし。
        }
    }

    if (want_read == socket_watch_read_ && want_write == socket_watch_write_)
        return Result<void>();

    socket_watch_read_ = want_read;
    socket_watch_write_ = want_write;

    Result<void> u = controller_.updateWatch(fd, want_read, want_write);
    if (u.isError())
        return Result<void>(ERROR, u.getErrorMessage());
    return Result<void>();
}

Result<void> HttpSession::prepareResponseOrCgi_()
{
    // RequestProcessorでレスポンスを確定（CGIはここでは扱わない）
    response_.reset();

    // ここで next_step 等を確定させる
    Result<void> ready = handler_.onRequestReady();
    if (ready.isError())
    {
        Result<void> er =
            setSimpleErrorResponse(response_, http::HttpStatus::BAD_REQUEST);
        if (er.isError())
            return er;
        response_.setHttpVersion(request_.getHttpVersion());
    }
    else
    {
        response_.setHttpVersion(request_.getHttpVersion());
    }

    if (handler_.getNextStep() == HttpHandler::EXECUTE_CGI)
    {
        Result<void> sr = startCgi_();
        if (sr.isError())
        {
            RequestProcessor::Output out;
            Result<RequestProcessor::Output> per = processor_.processError(
                request_, http::HttpStatus::SERVER_ERROR, response_);
            if (per.isOk())
            {
                out = per.unwrap();
            }
            else
            {
                Result<void> er = setSimpleErrorResponse(
                    response_, http::HttpStatus::SERVER_ERROR);
                if (er.isError())
                    return er;
                out.body_source = NULL;
                out.should_close_connection = false;
            }
            response_.setHttpVersion(request_.getHttpVersion());
            if (response_writer_ != NULL)
            {
                delete response_writer_;
                response_writer_ = NULL;
            }
            if (body_source_ != NULL)
            {
                delete body_source_;
                body_source_ = NULL;
            }

            body_source_ = out.body_source;
            http::HttpResponseEncoder::Options opt =
                makeEncoderOptions(request_);
            response_writer_ =
                new HttpResponseWriter(response_, opt, body_source_);

            connection_close_ =
                connection_close_ || out.should_close_connection ||
                !request_.shouldKeepAlive() || handler_.shouldCloseConnection();
            has_response_ = true;
            state_ = SEND_RESPONSE;
            (void)updateSocketWatches_();
            return Result<void>();
        }
        return Result<void>();
    }

    RequestProcessor::Output out;
    Result<RequestProcessor::Output> pr =
        processor_.process(request_, response_);
    if (pr.isError())
    {
        utils::Log::error("RequestProcessor error: ", pr.getErrorMessage());
        Result<RequestProcessor::Output> per = processor_.processError(
            request_, http::HttpStatus::SERVER_ERROR, response_);
        if (per.isOk())
        {
            out = per.unwrap();
        }
        else
        {
            Result<void> er = setSimpleErrorResponse(
                response_, http::HttpStatus::SERVER_ERROR);
            if (er.isError())
                return er;
            out.body_source = NULL;
            out.should_close_connection = false;
        }
        response_.setHttpVersion(request_.getHttpVersion());
        out.should_close_connection = true;
    }
    else
    {
        out = pr.unwrap();
    }

    if (body_source_ != NULL)
    {
        delete body_source_;
        body_source_ = NULL;
    }
    body_source_ = out.body_source;

    if (response_writer_ != NULL)
    {
        delete response_writer_;
        response_writer_ = NULL;
    }

    http::HttpResponseEncoder::Options opt = makeEncoderOptions(request_);
    response_writer_ = new HttpResponseWriter(response_, opt, body_source_);

    connection_close_ = connection_close_ || out.should_close_connection ||
                        !request_.shouldKeepAlive() ||
                        handler_.shouldCloseConnection();

    has_request_ = true;
    has_response_ = true;
    state_ = SEND_RESPONSE;
    (void)updateSocketWatches_();
    return Result<void>();
}

Result<void> HttpSession::consumeRecvBufferWithoutRead_()
{
    if (state_ != RECV_REQUEST)
        return Result<void>();
    if (is_complete_)
        return Result<void>();

    // recv_buffer_ の中だけで進める（read()はしない）。
    for (;;)
    {
        const size_t before = recv_buffer_.size();

        Result<void> c = handler_.consumeFromRecvBuffer(recv_buffer_);
        if (c.isError())
        {
            http::HttpStatus st = request_.getParseErrorStatus();
            if (st == http::HttpStatus::OK)
                st = http::HttpStatus::BAD_REQUEST;

            Result<RequestProcessor::Output> pr =
                processor_.processError(request_, st, response_);
            if (pr.isError())
                return Result<void>(ERROR, pr.getErrorMessage());
            RequestProcessor::Output out = pr.unwrap();

            response_.setHttpVersion(request_.getHttpVersion());

            if (response_writer_ != NULL)
            {
                delete response_writer_;
                response_writer_ = NULL;
            }
            if (body_source_ != NULL)
            {
                delete body_source_;
                body_source_ = NULL;
            }

            body_source_ = out.body_source;

            http::HttpResponseEncoder::Options opt =
                makeEncoderOptions(request_);
            response_writer_ =
                new HttpResponseWriter(response_, opt, body_source_);
            has_response_ = true;
            state_ = SEND_RESPONSE;
            (void)updateSocketWatches_();
            return Result<void>();
        }

        if (request_.isParseComplete())
        {
            return prepareResponseOrCgi_();
        }

        const size_t after = recv_buffer_.size();
        if (after >= before)
        {
            // これ以上は進められない（追加入力待ち or パーサ内部状態待ち）
            break;
        }
    }
    return Result<void>();
}

static std::string dirnameOf_(const std::string& path)
{
    const std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return std::string();
    if (pos == 0)
        return std::string("/");
    return path.substr(0, pos);
}

Result<http::HttpRequest> HttpSession::buildInternalRedirectRequest_(
    const std::string& uri_path) const
{
    http::HttpRequest req;
    std::string raw;
    raw += "GET ";
    raw += uri_path;
    raw += " HTTP/1.";
    raw += static_cast<char>('0' + request_.getMinorVersion());
    raw += "\r\n";

    // Host は可能なら維持
    Result<const std::vector<std::string>&> host =
        request_.getHeader(http::HeaderName(http::HeaderName::HOST).toString());
    if (host.isOk() && !host.unwrap().empty())
    {
        raw += "Host: ";
        raw += host.unwrap()[0];
        raw += "\r\n";
    }
    raw += "\r\n";

    std::vector<utils::Byte> buf;
    buf.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
        buf.push_back(static_cast<utils::Byte>(raw[i]));

    Result<size_t> parsed = req.parse(buf);
    if (parsed.isError() || !req.isParseComplete())
    {
        return Result<http::HttpRequest>(ERROR, http::HttpRequest(),
            "failed to build internal redirect request");
    }
    return req;
}

Result<void> HttpSession::startCgi_()
{
    if (!handler_.hasLocationRouting())
        return Result<void>(ERROR, "missing routing for CGI");

    Result<server::CgiContext> ctxr =
        handler_.getLocationRouting().getCgiContext();
    if (ctxr.isError())
        return Result<void>(ERROR, ctxr.getErrorMessage());
    const server::CgiContext ctx = ctxr.unwrap();

    // Request body fd
    int request_body_fd = -1;
    if (request_.hasBody())
    {
        (void)handler_.bodyStore().finish();
        Result<int> fd = handler_.bodyStore().openForRead();
        if (fd.isOk())
            request_body_fd = fd.unwrap();
    }

    // CGI env
    http::CgiMetaVariables meta = http::CgiMetaVariables::fromHttpRequest(
        request_, ctx.script_name, ctx.path_info);

    // 追加のメタ変数（最小限）
    meta.setServerName(socket_fd_.getServerIp().toString());
    {
        std::istringstream iss(socket_fd_.getServerPort().toString());
        unsigned int p = 0;
        iss >> p;
        meta.setServerPort(p);
    }
    meta.setRemoteAddr(socket_fd_.getClientIp().toString());
    meta.setServerSoftware("webserv");

    std::map<std::string, std::string> env = meta.getAll();
    env["SCRIPT_FILENAME"] = ctx.script_filename.str();
    env["QUERY_STRING"] = ctx.query_string;

    // 実行: executor を execve し、script_filename を引数に渡す
    std::vector<std::string> args;
    args.push_back(ctx.script_filename.str());

    const std::string working_dir = dirnameOf_(ctx.script_filename.str());
    Result<CgiSpawnResult> spawned = server::CgiPipeFd::Execute(
        ctx.executor_path.str(), args, env, working_dir);
    if (spawned.isError())
        return Result<void>(ERROR, spawned.getErrorMessage());

    const CgiSpawnResult s = spawned.unwrap();
    active_cgi_session_ = new CgiSession(s.pid, s.stdin_fd, s.stdout_fd,
        s.stderr_fd, request_body_fd, this, controller_);

    Result<void> d = controller_.delegateSession(active_cgi_session_);
    if (d.isError())
    {
        delete active_cgi_session_;
        active_cgi_session_ = NULL;
        return Result<void>(ERROR, d.getErrorMessage());
    }

    waiting_for_cgi_ = true;
    state_ = EXECUTE_CGI;
    (void)updateSocketWatches_();
    return Result<void>();
}

Result<void> HttpSession::handleEvent(const FdEvent& event)
{
    updateLastActiveTime();

    if (is_complete_)
        return Result<void>();

    if (event.type == kTimeoutEvent)
    {
        is_complete_ = true;
        socket_fd_.shutdown();
        controller_.requestDelete(this);
        return Result<void>();
    }

    if (event.type == kErrorEvent)
    {
        is_complete_ = true;
        socket_fd_.shutdown();
        controller_.requestDelete(this);
        return Result<void>();
    }

    if (event.is_opposite_close)
    {
        // クライアント側のclose検出（自分のソケットのみ）
        if (event.fd == socket_fd_.getFd())
            connection_close_ = true;
    }

    // CGI 実行中はソケットイベントを基本的に無視し、CGI からの通知を待つ。
    if (state_ == EXECUTE_CGI)
    {
        if (event.fd == socket_fd_.getFd() && event.type == kReadEvent)
        {
            if (recv_buffer_.size() < kMaxRecvBufferBytes)
            {
                const ssize_t n = recv_buffer_.fillFromFd(socket_fd_.getFd());
                if (n == 0)
                    connection_close_ = true;
            }
            (void)updateSocketWatches_();
        }

        if (connection_close_)
        {
            is_complete_ = true;
            socket_fd_.shutdown();

            // クライアントが消えたので CGI も中断
            if (active_cgi_session_ != NULL)
            {
                controller_.requestDelete(active_cgi_session_);
                active_cgi_session_ = NULL;
            }

            controller_.requestDelete(this);
        }
        return Result<void>();
    }

    // --- RECV ---
    if (state_ == RECV_REQUEST && event.type == kReadEvent)
    {
        for (;;)
        {
            // まず、すでにバッファに溜まっている分を先に消費する。
            Result<void> c = handler_.consumeFromRecvBuffer(recv_buffer_);
            if (c.isError())
            {
                // パースエラーは 4xx を返す（最小）
                http::HttpStatus st = request_.getParseErrorStatus();
                if (st == http::HttpStatus::OK)
                    st = http::HttpStatus::BAD_REQUEST;

                Result<RequestProcessor::Output> pr =
                    processor_.processError(request_, st, response_);
                if (pr.isError())
                    return Result<void>(ERROR, pr.getErrorMessage());
                RequestProcessor::Output out = pr.unwrap();

                // リクエストのHTTPバージョンを反映（できる範囲で）
                response_.setHttpVersion(request_.getHttpVersion());

                if (response_writer_ != NULL)
                {
                    delete response_writer_;
                    response_writer_ = NULL;
                }
                if (body_source_ != NULL)
                {
                    delete body_source_;
                    body_source_ = NULL;
                }

                body_source_ = out.body_source;

                http::HttpResponseEncoder::Options opt =
                    makeEncoderOptions(request_);
                response_writer_ =
                    new HttpResponseWriter(response_, opt, body_source_);
                has_response_ = true;
                state_ = SEND_RESPONSE;
                (void)updateSocketWatches_();
                break;
            }

            if (request_.isParseComplete())
            {
                Result<void> pr = prepareResponseOrCgi_();
                if (pr.isError())
                    return pr;
                break;
            }

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
                is_complete_ = true;
                socket_fd_.shutdown();
                controller_.requestDelete(this);
                return Result<void>();
            }
        }

        // recv_buffer_ サイズに応じた read watch のON/OFF を反映する。
        (void)updateSocketWatches_();
    }

    // --- SEND ---
    if (state_ == SEND_RESPONSE && event.type == kWriteEvent)
    {
        if (response_writer_ == NULL)
        {
            is_complete_ = true;
            return Result<void>(ERROR, "missing response writer");
        }

        // 送信バッファが空なら積む
        if (send_buffer_.size() == 0)
        {
            Result<HttpResponseWriter::PumpResult> pumped =
                response_writer_->pump(send_buffer_);
            if (pumped.isError())
            {
                is_complete_ = true;
                return Result<void>(ERROR, pumped.getErrorMessage());
            }

            const HttpResponseWriter::PumpResult pr = pumped.unwrap();
            if (pr.should_close_connection)
                connection_close_ = true;

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

            has_request_ = false;
            has_response_ = false;

            if (connection_close_)
            {
                is_complete_ = true;
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
            connection_close_ = false;
            waiting_for_cgi_ = false;

            // active_cgi_session_ は controller 所有。ここでは delete しない。

            // keep-alive: 次のリクエストへ
            state_ = RECV_REQUEST;
            (void)updateSocketWatches_();

            // pipelining 等で user-space に次リクエストが残っている場合は、
            // イベント無しでも進められる分だけ処理する。
            (void)consumeRecvBufferWithoutRead_();
        }
    }

    return Result<void>();
}

Result<void> HttpSession::onCgiHeadersReady(CgiSession& cgi)
{
    // Local Redirect の場合は内部GETとして再投入
    const http::CgiResponse& cr = cgi.response();
    const http::CgiResponseType t = cr.getResponseType();

    if (t == http::kLocalRedirect)
    {
        if (redirect_count_ >= kMaxInternalRedirects)
        {
            // stdout は不要。詰まりを防ぐため閉じる。
            const int out_fd = cgi.releaseStdoutFd();
            if (out_fd >= 0)
                ::close(out_fd);

            if (active_cgi_session_ == &cgi)
            {
                controller_.requestDelete(active_cgi_session_);
                active_cgi_session_ = NULL;
            }

            RequestProcessor::Output out;
            Result<RequestProcessor::Output> per = processor_.processError(
                request_, http::HttpStatus::SERVER_ERROR, response_);
            if (per.isOk())
            {
                out = per.unwrap();
            }
            else
            {
                Result<void> er = setSimpleErrorResponse(
                    response_, http::HttpStatus::SERVER_ERROR);
                if (er.isError())
                    return er;
                out.body_source = NULL;
                out.should_close_connection = false;
            }
            response_.setHttpVersion(request_.getHttpVersion());

            http::HttpResponseEncoder::Options opt =
                makeEncoderOptions(request_);
            if (response_writer_ != NULL)
                delete response_writer_;
            if (body_source_ != NULL)
                delete body_source_;
            response_writer_ = NULL;
            body_source_ = NULL;

            body_source_ = out.body_source;
            response_writer_ =
                new HttpResponseWriter(response_, opt, body_source_);

            connection_close_ =
                connection_close_ || out.should_close_connection ||
                !request_.shouldKeepAlive() || handler_.shouldCloseConnection();

            waiting_for_cgi_ = false;
            has_response_ = true;
            state_ = SEND_RESPONSE;
            (void)updateSocketWatches_();
            return Result<void>();
        }

        Result<std::string> loc = cr.getLocalRedirectUriPath();
        if (loc.isError())
            return Result<void>(ERROR, loc.getErrorMessage());

        // stdout は不要。以後 CGI が詰まらないように止める。
        const int out_fd = cgi.releaseStdoutFd();
        if (out_fd >= 0)
            ::close(out_fd);

        // いまの CGI は dispatch バッチ末尾で安全に delete する
        if (active_cgi_session_ == &cgi)
        {
            controller_.requestDelete(active_cgi_session_);
            active_cgi_session_ = NULL;
        }

        redirect_count_++;

        // internal redirect request を生成して再処理
        Result<http::HttpRequest> rr =
            buildInternalRedirectRequest_(loc.unwrap());
        if (rr.isError())
            return Result<void>(ERROR, rr.getErrorMessage());

        response_.reset();
        handler_.reset();
        request_ = rr.unwrap();

        // ここで stall しないよう、そのまま次のリクエスト処理へ進める
        Result<void> ready = handler_.onRequestReady();
        if (ready.isError())
        {
            Result<void> er = setSimpleErrorResponse(
                response_, http::HttpStatus::BAD_REQUEST);
            if (er.isError())
                return er;
            response_.setHttpVersion(request_.getHttpVersion());
        }
        else
        {
            response_.setHttpVersion(request_.getHttpVersion());
        }

        if (handler_.getNextStep() == HttpHandler::EXECUTE_CGI)
        {
            return startCgi_();
        }

        RequestProcessor::Output out;
        Result<RequestProcessor::Output> pr =
            processor_.process(request_, response_);
        if (pr.isError())
        {
            utils::Log::error("RequestProcessor error: ", pr.getErrorMessage());
            Result<RequestProcessor::Output> per = processor_.processError(
                request_, http::HttpStatus::SERVER_ERROR, response_);
            if (per.isOk())
            {
                out = per.unwrap();
            }
            else
            {
                Result<void> er = setSimpleErrorResponse(
                    response_, http::HttpStatus::SERVER_ERROR);
                if (er.isError())
                    return er;
                out.body_source = NULL;
                out.should_close_connection = false;
            }
            response_.setHttpVersion(request_.getHttpVersion());
            out.should_close_connection = true;
        }
        else
        {
            out = pr.unwrap();
        }

        if (body_source_ != NULL)
        {
            delete body_source_;
            body_source_ = NULL;
        }
        body_source_ = out.body_source;

        if (response_writer_ != NULL)
        {
            delete response_writer_;
            response_writer_ = NULL;
        }

        http::HttpResponseEncoder::Options opt = makeEncoderOptions(request_);
        response_writer_ = new HttpResponseWriter(response_, opt, body_source_);

        connection_close_ = connection_close_ || out.should_close_connection ||
                            !request_.shouldKeepAlive() ||
                            handler_.shouldCloseConnection();

        waiting_for_cgi_ = false;
        has_response_ = true;
        state_ = SEND_RESPONSE;
        (void)updateSocketWatches_();
        return Result<void>();
    }

    // 通常: CGI ヘッダを HTTP Response に適用して、body は stdout を流す
    response_.reset();
    response_.setHttpVersion(request_.getHttpVersion());

    Result<void> ar = cr.applyToHttpResponse(response_);
    if (ar.isError())
    {
        // CGIヘッダが壊れている場合は CGI 出力を捨てて 500 を返す。
        const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
        (void)prefetched;
        const int stdout_fd = cgi.releaseStdoutFd();
        if (stdout_fd >= 0)
            ::close(stdout_fd);

        if (active_cgi_session_ == &cgi)
        {
            controller_.requestDelete(active_cgi_session_);
            active_cgi_session_ = NULL;
        }

        RequestProcessor::Output out;
        Result<RequestProcessor::Output> per = processor_.processError(
            request_, http::HttpStatus::SERVER_ERROR, response_);
        if (per.isOk())
        {
            out = per.unwrap();
        }
        else
        {
            (void)setSimpleErrorResponse(
                response_, http::HttpStatus::SERVER_ERROR);
            out.body_source = NULL;
            out.should_close_connection = false;
        }
        response_.setHttpVersion(request_.getHttpVersion());

        if (body_source_ != NULL)
        {
            delete body_source_;
            body_source_ = NULL;
        }
        body_source_ = out.body_source;

        if (response_writer_ != NULL)
        {
            delete response_writer_;
            response_writer_ = NULL;
        }

        http::HttpResponseEncoder::Options opt = makeEncoderOptions(request_);
        response_writer_ = new HttpResponseWriter(response_, opt, body_source_);

        connection_close_ = connection_close_ || out.should_close_connection ||
                            !request_.shouldKeepAlive() ||
                            handler_.shouldCloseConnection();

        waiting_for_cgi_ = false;
        has_response_ = true;
        state_ = SEND_RESPONSE;
        (void)updateSocketWatches_();
        return Result<void>();
    }

    const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd < 0)
    {
        return Result<void>(ERROR, "missing cgi stdout fd");
    }

    if (body_source_ != NULL)
    {
        delete body_source_;
        body_source_ = NULL;
    }
    body_source_ = new PrefetchedFdBodySource(stdout_fd, prefetched);

    if (response_writer_ != NULL)
    {
        delete response_writer_;
        response_writer_ = NULL;
    }

    http::HttpResponseEncoder::Options opt = makeEncoderOptions(request_);
    response_writer_ = new HttpResponseWriter(response_, opt, body_source_);

    waiting_for_cgi_ = false;
    has_response_ = true;
    state_ = SEND_RESPONSE;
    (void)updateSocketWatches_();

    // 以後 CgiSession は controller が所有/削除する。HttpSession
    // 側では触らない。
    if (active_cgi_session_ == &cgi)
        active_cgi_session_ = NULL;
    return Result<void>();
}

Result<void> HttpSession::onCgiError(
    CgiSession& cgi, const std::string& message)
{
    if (is_complete_)
        return Result<void>();

    // CGI 実行中以外に通知が来た場合は安全に無視する。
    if (state_ != EXECUTE_CGI)
        return Result<void>();

    utils::Log::error("CGI error: ", message);

    // CGI の stdout を閉じて詰まり/イベントループを防ぐ。
    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd >= 0)
        ::close(stdout_fd);

    // CgiSession は controller が所有。ここで削除要求だけ出す。
    if (active_cgi_session_ == &cgi)
    {
        controller_.requestDelete(active_cgi_session_);
        active_cgi_session_ = NULL;
    }
    else
    {
        controller_.requestDelete(&cgi);
    }

    response_.reset();
    response_.setHttpVersion(request_.getHttpVersion());

    RequestProcessor::Output out;
    Result<RequestProcessor::Output> per = processor_.processError(
        request_, http::HttpStatus::SERVER_ERROR, response_);
    if (per.isOk())
    {
        out = per.unwrap();
    }
    else
    {
        (void)setSimpleErrorResponse(response_, http::HttpStatus::SERVER_ERROR);
        out.body_source = NULL;
        out.should_close_connection = false;
    }
    response_.setHttpVersion(request_.getHttpVersion());

    if (body_source_ != NULL)
    {
        delete body_source_;
        body_source_ = NULL;
    }
    body_source_ = out.body_source;

    if (response_writer_ != NULL)
    {
        delete response_writer_;
        response_writer_ = NULL;
    }

    http::HttpResponseEncoder::Options opt = makeEncoderOptions(request_);
    response_writer_ = new HttpResponseWriter(response_, opt, body_source_);

    connection_close_ = connection_close_ || out.should_close_connection ||
                        !request_.shouldKeepAlive() ||
                        handler_.shouldCloseConnection();

    waiting_for_cgi_ = false;
    has_response_ = true;
    state_ = SEND_RESPONSE;
    (void)updateSocketWatches_();
    return Result<void>();
}

}  // namespace server
