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

// RequestProcessor::Output を使って context_.body_source / context_.response_writer
// を差し替える。 既存の writer/body_source は必要に応じて破棄する。
void HttpSession::installBodySourceAndWriter_(BodySource* body_source)
{
    if (context_.response_writer != NULL)
    {
        delete context_.response_writer;
        context_.response_writer = NULL;
    }
    if (context_.body_source != NULL)
    {
        delete context_.body_source;
        context_.body_source = NULL;
    }

    context_.body_source = body_source;

    http::HttpResponseEncoder::Options opt = makeEncoderOptions_(context_.request);
    context_.response_writer = new HttpResponseWriter(context_.response, opt, context_.body_source);
}

// processError を試み、失敗したら setSimpleErrorResponse_
// で最低限のヘッダだけ作る。 fallback の場合 out->body_source は NULL
// になる。
Result<void> HttpSession::buildErrorOutput_(
    http::HttpStatus status, RequestProcessor::Output* out)
{
    Result<RequestProcessor::Output> per =
        processor_.processError(context_.request, status, context_.response);
    if (per.isOk())
    {
        *out = per.unwrap();
        return Result<void>();
    }

    Result<void> er = setSimpleErrorResponse_(context_.response, status);
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
        processor_.process(context_.request, context_.response);
    if (pr.isOk())
    {
        *out = pr.unwrap();
        return Result<void>();
    }

    utils::Log::error("RequestProcessor error: ", pr.getErrorMessage());

    Result<void> bo = buildErrorOutput_(http::HttpStatus::SERVER_ERROR, out);
    if (bo.isError())
        return bo;

    context_.response.setHttpVersion(context_.request.getHttpVersion());
    out->should_close_connection = true;
    return Result<void>();
}

}  // namespace server