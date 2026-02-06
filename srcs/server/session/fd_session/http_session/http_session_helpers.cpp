#include "server/session/fd_session/http_session.hpp"

namespace server
{

using namespace utils::result;

// RequestProcessor::Output を使って context_.body_source /
// context_.response_writer を差し替える。 既存の writer/body_source
// は必要に応じて破棄する。
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

    http::HttpResponseEncoder::Options opt =
        module_.makeEncoderOptions(context_.request);
    context_.response_writer =
        new HttpResponseWriter(context_.response, opt, context_.body_source);
}

// processError を試み、失敗したら setSimpleErrorResponse_
// で最低限のヘッダだけ作る。 fallback の場合 out->body_source は NULL
// になる。
Result<void> HttpSession::buildErrorOutput_(
    http::HttpStatus status, RequestProcessor::Output* out)
{
    return module_.buildErrorOutput(context_, status, out);
}

Result<void> HttpSession::buildProcessorOutputOrServerError_(
    RequestProcessor::Output* out)
{
    return module_.buildProcessorOutputOrServerError(context_, out);
}

}  // namespace server