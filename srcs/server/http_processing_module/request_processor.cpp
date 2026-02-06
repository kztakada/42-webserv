#include "server/http_processing_module/request_processor.hpp"

#include "server/http_processing_module/request_processor/action_handler_factory.hpp"

namespace server
{

using utils::result::Result;

RequestProcessor::RequestProcessor(const RequestRouter& router)
    : router_(router),
      autoindex_renderer_(),
      error_renderer_(),
      internal_redirect_(),
      file_responder_(),
      handler_factory_(router_, autoindex_renderer_, error_renderer_,
          internal_redirect_, file_responder_)
{
}

RequestProcessor::~RequestProcessor() {}

Result<RequestProcessor::Output> RequestProcessor::process(
    const http::HttpRequest& request, const IPAddress& server_ip,
    const PortType& server_port, http::HttpResponse& out_response)
{
    // 内部リダイレクトを最小限サポートする（error_page の内部URI等）
    ProcessingState state(request);
    for (int redirect_guard = 0; redirect_guard < 5; ++redirect_guard)
    {
        Result<LocationRouting> route_result =
            router_.route(state.current, server_ip, server_port);
        if (route_result.isError())
            return Result<Output>(ERROR, route_result.getErrorMessage());

        LocationRouting route = route_result.unwrap();
        const ActionType action = route.getNextAction();

        ActionHandler* handler = handler_factory_.getHandler(action);
        if (handler == NULL)
        {
            Result<RequestProcessor::Output> r = error_renderer_.respond(
                http::HttpStatus::NOT_IMPLEMENTED, out_response);
            if (r.isError())
                return Result<Output>(ERROR, r.getErrorMessage());
            return r.unwrap();
        }

        Result<HandlerResult> handled = handler->handle(
            route, server_ip, server_port, out_response, &state);
        if (handled.isError())
            return Result<Output>(ERROR, handled.getErrorMessage());

        HandlerResult result = handled.unwrap();
        if (result.should_continue)
        {
            // 内部リダイレクト（error_page の内部URI等）で次のリクエストへ
            // 進む場合、途中まで構築されたレスポンスを引き継がない。
            out_response.reset();
            state.current = result.next_request;
            continue;
        }
        return result.output;
    }

    return Result<Output>(ERROR, "too many internal redirects");
}

Result<RequestProcessor::Output> RequestProcessor::processError(
    const http::HttpRequest& request, const IPAddress& server_ip,
    const PortType& server_port, const http::HttpStatus& error_status,
    http::HttpResponse& out_response)
{
    // まずは設定に沿った error_page（内部URI）を探す。
    std::string target;
    Result<LocationRouting> route_result =
        router_.route(request, server_ip, server_port);
    if (route_result.isOk())
    {
        LocationRouting route = route_result.unwrap();
        if (route.tryGetErrorPagePath(error_status, &target))
        {
            if (!target.empty() && target[0] == '/')
            {
                Result<http::HttpRequest> ir =
                    internal_redirect_.buildInternalGetRequest(target, request);
                if (ir.isOk())
                {
                    out_response.reset();
                    Result<Output> pr = process(
                        ir.unwrap(), server_ip, server_port, out_response);
                    if (pr.isOk() && out_response.getStatus().isSuccess())
                    {
                        // コンテンツは内部URIの結果を使い、ステータスだけ元の
                        // エラーに戻す。
                        (void)out_response.setStatus(error_status);
                        return pr.unwrap();
                    }
                }
            }
        }
    }

    out_response.reset();
    return error_renderer_.respond(error_status, out_response);
}

}  // namespace server
