#include <fcntl.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>

#include "server/session/fd_session/http_session.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

http::HttpResponseEncoder::Options HttpSession::makeEncoderOptions_(
    const http::HttpRequest& request)
{
    http::HttpResponseEncoder::Options opt;
    opt.request_minor_version = request.getMinorVersion();
    opt.request_should_keep_alive = request.shouldKeepAlive();
    opt.request_is_head = (request.getMethod() == http::HttpMethod::HEAD);
    return opt;
}

Result<void> HttpSession::setSimpleErrorResponse_(
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

// RequestProcessor::Output を使って body_source_ / response_writer_
// を差し替える。 既存の writer/body_source は必要に応じて破棄する。
void HttpSession::installBodySourceAndWriter_(BodySource* body_source)
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

    body_source_ = body_source;

    http::HttpResponseEncoder::Options opt = makeEncoderOptions_(request_);
    response_writer_ = new HttpResponseWriter(response_, opt, body_source_);
}

// processError を試み、失敗したら setSimpleErrorResponse_
// で最低限のヘッダだけ作る。 fallback の場合 out->body_source は NULL
// になる。
Result<void> HttpSession::buildErrorOutput_(
    http::HttpStatus status, RequestProcessor::Output* out)
{
    Result<RequestProcessor::Output> per =
        processor_.processError(request_, status, response_);
    if (per.isOk())
    {
        *out = per.unwrap();
        return Result<void>();
    }

    Result<void> er = setSimpleErrorResponse_(response_, status);
    if (er.isError())
        return er;

    out->body_source = NULL;
    out->should_close_connection = false;
    return Result<void>();
}

Result<void> HttpSession::buildProcessorOutputOrServerError_(
    RequestProcessor::Output* out)
{
    Result<RequestProcessor::Output> pr =
        processor_.process(request_, response_);
    if (pr.isOk())
    {
        *out = pr.unwrap();
        return Result<void>();
    }

    utils::Log::error("RequestProcessor error: ", pr.getErrorMessage());

    Result<void> bo = buildErrorOutput_(http::HttpStatus::SERVER_ERROR, out);
    if (bo.isError())
        return bo;

    response_.setHttpVersion(request_.getHttpVersion());
    out->should_close_connection = true;
    return Result<void>();
}

}  // namespace server