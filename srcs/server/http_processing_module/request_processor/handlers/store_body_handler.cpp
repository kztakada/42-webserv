#include "server/http_processing_module/request_processor/handlers/store_body_handler.hpp"

#include "server/http_processing_module/request_processor/error_page_renderer.hpp"

namespace server
{

using utils::result::Result;

Result<HandlerResult> StoreBodyHandler::handle(const LocationRouting& route,
    const IPAddress& server_ip, const PortType& server_port,
    http::HttpResponse& out_response, ProcessingState* state)
{
    (void)server_ip;
    (void)server_port;

    if (state == NULL)
        return Result<HandlerResult>(ERROR, "state is null");

    // upload_store に保存済み（body は Session/BodyStore
    // 側で書き込み済み） ここでは結果レスポンスだけ生成する。
    if (state->current.getMethod() != http::HttpMethod::POST)
    {
        Result<RequestProcessorOutput> r =
            renderer_.respond(http::HttpStatus::NOT_ALLOWED, out_response);
        if (r.isError())
            return Result<HandlerResult>(ERROR, r.getErrorMessage());
        HandlerResult res;
        res.output = r.unwrap();
        return res;
    }

    Result<UploadContext> up = route.getUploadContext();
    if (up.isError())
    {
        Result<RequestProcessorOutput> r =
            renderer_.respond(http::HttpStatus::BAD_REQUEST, out_response);
        if (r.isError())
            return Result<HandlerResult>(ERROR, r.getErrorMessage());
        HandlerResult res;
        res.output = r.unwrap();
        return res;
    }

    Result<void> s = out_response.setStatus(http::HttpStatus::CREATED);
    if (s.isError())
        return Result<HandlerResult>(ERROR, s.getErrorMessage());

    HandlerResult res;
    (void)out_response.setExpectedContentLength(0);
    res.output.body_source = NULL;
    res.output.should_close_connection = false;
    return res;
}

}  // namespace server
