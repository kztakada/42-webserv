#include "server/session/fd_session/http_session.hpp"

#include "http/header.hpp"
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

Result<void> HttpSession::handleEvent(const FdEvent& event)
{
    updateLastActiveTime();

    if (is_complete_)
        return Result<void>();

    if (event.type == kTimeoutEvent)
    {
        is_complete_ = true;
        socket_fd_.shutdown();
        return Result<void>();
    }

    if (event.type == kErrorEvent)
    {
        is_complete_ = true;
        socket_fd_.shutdown();
        return Result<void>();
    }

    if (event.is_opposite_close)
    {
        // クライアント側のclose検出
        connection_close_ = true;
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
                Result<void> er = setSimpleErrorResponse(response_, st);
                if (er.isError())
                    return er;

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

                http::HttpResponseEncoder::Options opt =
                    makeEncoderOptions(request_);
                response_writer_ = new HttpResponseWriter(response_, opt, NULL);
                has_response_ = true;
                state_ = SEND_RESPONSE;
                break;
            }

            if (request_.isParseComplete())
            {
                // RequestProcessorでレスポンスを確定（CGIはここでは扱わない）
                response_.reset();

                // ここで next_step 等を確定させる（現状はSEND_RESPONSEのみ）
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

                RequestProcessor::Output out;
                Result<RequestProcessor::Output> pr =
                    processor_.process(request_, response_);
                if (pr.isError())
                {
                    utils::Log::error(
                        "RequestProcessor error: ", pr.getErrorMessage());
                    Result<void> er = setSimpleErrorResponse(
                        response_, http::HttpStatus::SERVER_ERROR);
                    if (er.isError())
                        return er;
                    response_.setHttpVersion(request_.getHttpVersion());
                    out.body_source = NULL;
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

                http::HttpResponseEncoder::Options opt =
                    makeEncoderOptions(request_);
                response_writer_ =
                    new HttpResponseWriter(response_, opt, body_source_);

                connection_close_ = connection_close_ ||
                                    out.should_close_connection ||
                                    !request_.shouldKeepAlive() ||
                                    handler_.shouldCloseConnection();

                has_request_ = true;
                has_response_ = true;
                state_ = SEND_RESPONSE;
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
                return Result<void>();
            }
        }
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
            connection_close_ = false;
            waiting_for_cgi_ = false;

            if (connection_close_)
            {
                is_complete_ = true;
                socket_fd_.shutdown();
                return Result<void>();
            }

            // keep-alive: 次のリクエストへ
            state_ = RECV_REQUEST;
        }
    }

    return Result<void>();
}

}  // namespace server
