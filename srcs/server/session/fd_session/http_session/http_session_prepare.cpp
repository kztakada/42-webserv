#include "server/session/fd_session/http_session.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::prepareResponseOrCgi_()
{
    // RequestProcessorでレスポンスを確定（CGIはここでは扱わない）
    response_.reset();

    // ここで next_step 等を確定させる
    Result<void> rr = prepareRequestReadyOrBadRequest_();
    if (rr.isError())
        return rr;

    Result<void> fu = finalizeUploadStoreIfNeeded_();
    if (fu.isError())
    {
        RequestProcessor::Output out;
        Result<void> bo =
            buildErrorOutput_(http::HttpStatus::BAD_REQUEST, &out);
        if (bo.isError())
            return bo;
        response_.setHttpVersion(request_.getHttpVersion());
        installBodySourceAndWriter_(out.body_source);

        should_close_connection_ = should_close_connection_ || peer_closed_ ||
                                   out.should_close_connection ||
                                   !request_.shouldKeepAlive() ||
                                   handler_.shouldCloseConnection();
        state_ = SEND_RESPONSE;
        (void)updateSocketWatches_();
        return Result<void>();
    }

    if (handler_.getNextStep() == HttpHandler::EXECUTE_CGI)
    {
        Result<void> sr = startCgi_();
        if (sr.isError())
        {
            RequestProcessor::Output out;
            Result<void> bo =
                buildErrorOutput_(http::HttpStatus::SERVER_ERROR, &out);
            if (bo.isError())
                return bo;
            response_.setHttpVersion(request_.getHttpVersion());
            installBodySourceAndWriter_(out.body_source);

            should_close_connection_ =
                should_close_connection_ || peer_closed_ ||
                out.should_close_connection || !request_.shouldKeepAlive() ||
                handler_.shouldCloseConnection();
            state_ = SEND_RESPONSE;
            (void)updateSocketWatches_();
            return Result<void>();
        }
        return Result<void>();
    }

    RequestProcessor::Output out;
    Result<void> po = buildProcessorOutputOrServerError_(&out);
    if (po.isError())
        return po;

    installBodySourceAndWriter_(out.body_source);

    should_close_connection_ = should_close_connection_ || peer_closed_ ||
                               out.should_close_connection ||
                               !request_.shouldKeepAlive() ||
                               handler_.shouldCloseConnection();
    state_ = SEND_RESPONSE;
    (void)updateSocketWatches_();
    return Result<void>();
}

Result<void> HttpSession::consumeRecvBufferWithoutRead_()
{
    if (state_ != RECV_REQUEST)
        return Result<void>();
    if (state_ == CLOSE_WAIT)
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

            RequestProcessor::Output out;
            Result<void> bo = buildErrorOutput_(st, &out);
            if (bo.isError())
                return bo;

            response_.setHttpVersion(request_.getHttpVersion());

            installBodySourceAndWriter_(out.body_source);
            state_ = SEND_RESPONSE;
            (void)updateSocketWatches_();
            return Result<void>();
        }

        if (request_.isParseComplete())
            return prepareResponseOrCgi_();

        const size_t after = recv_buffer_.size();

        // これ以上は進められない（追加入力待ち or パーサ内部状態待ち）
        if (after >= before)
            break;
    }
    return Result<void>();
}

}  // namespace server
