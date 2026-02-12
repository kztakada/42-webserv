#include "server/http_processing_module/request_processor/handlers/internal_redirect_handler.hpp"

namespace server
{

using utils::result::Result;

Result<HandlerResult> InternalRedirectHandler::handle(
    const LocationRouting& route, const IPAddress& server_ip,
    const PortType& server_port, http::HttpResponse& out_response,
    ProcessingState* state)
{
    (void)server_ip;
    (void)server_port;
    (void)out_response;

    if (state == NULL)
        return Result<HandlerResult>(ERROR, "state is null");

    // error_page の内部リダイレクトは「コンテンツだけ差し替えて、
    // 元のエラーステータスは維持する」挙動にする。
    if (!state->has_preserved_error_status && route.getHttpStatus().isError())
    {
        state->has_preserved_error_status = true;
        state->preserved_error_status = route.getHttpStatus();
    }

    // 405 の場合は Allow ヘッダーも維持する。
    if (!state->has_preserved_allow_header &&
        route.getHttpStatus() == http::HttpStatus::NOT_ALLOWED)
    {
        Result<std::string> allow = route.getAllowHeaderValue();
        if (allow.isOk() && !allow.unwrap().empty())
        {
            state->has_preserved_allow_header = true;
            state->preserved_allow_header_value = allow.unwrap();
        }
    }

    Result<http::HttpRequest> r = route.getInternalRedirectRequest();
    if (r.isError())
        return Result<HandlerResult>(ERROR, r.getErrorMessage());

    HandlerResult res;
    res.should_continue = true;
    res.next_request = r.unwrap();
    return res;
}

}  // namespace server
