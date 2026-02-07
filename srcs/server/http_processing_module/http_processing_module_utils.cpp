#include "server/http_processing_module/http_processing_module.hpp"
#include "server/session/fd_session/http_session/session_context.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

http::HttpResponseEncoder::Options HttpProcessingModule::makeEncoderOptions(
    const http::HttpRequest& request)
{
    http::HttpResponseEncoder::Options opt;
    opt.request_minor_version = request.getMinorVersion();
    opt.request_should_keep_alive = request.shouldKeepAlive();
    opt.request_is_head = (request.getMethod() == http::HttpMethod::HEAD);
    return opt;
}

Result<void> HttpProcessingModule::setSimpleErrorResponse(
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

Result<void> HttpProcessingModule::buildErrorOutput(SessionContext& context,
    http::HttpStatus status, RequestProcessor::Output* out)
{
    Result<RequestProcessor::Output> per =
        processor.processError(context.request, context.socket_fd.getServerIp(),
            context.socket_fd.getServerPort(), status, context.response);
    if (per.isOk())
    {
        *out = per.unwrap();
        return Result<void>();
    }

    Result<void> er = setSimpleErrorResponse(context.response, status);
    if (er.isError())
        return er;

    out->body_source.reset(NULL);
    out->should_close_connection = false;
    return Result<void>();
}

Result<void> HttpProcessingModule::buildProcessorOutputOrServerError(
    SessionContext& context, RequestProcessor::Output* out)
{
    Result<RequestProcessor::Output> pr =
        processor.process(context.request, context.socket_fd.getServerIp(),
            context.socket_fd.getServerPort(), context.response);
    if (pr.isOk())
    {
        // 成功時、201(Created) ならばアップロードが成功したとみなして
        // body_store を commit する。
        if (context.request.getMethod() == http::HttpMethod::POST &&
            context.response.getStatus() == http::HttpStatus::CREATED)
        {
            context.request_handler.bodyStore().commit();
        }
        *out = pr.unwrap();
        return Result<void>();
    }

    utils::Log::error("RequestProcessor error: ", pr.getErrorMessage());

    Result<void> bo =
        buildErrorOutput(context, http::HttpStatus::SERVER_ERROR, out);
    if (bo.isError())
        return bo;

    context.response.setHttpVersion(context.request.getHttpVersion());
    out->should_close_connection = true;
    return Result<void>();
}

}  // namespace server
