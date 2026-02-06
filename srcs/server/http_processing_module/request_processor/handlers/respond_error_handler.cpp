#include "server/http_processing_module/request_processor/handlers/respond_error_handler.hpp"

#include "server/http_processing_module/request_processor/error_page_renderer.hpp"

namespace server
{

using utils::result::Result;

Result<HandlerResult> RespondErrorHandler::handle(const LocationRouting& route,
    const IPAddress& server_ip, const PortType& server_port,
    http::HttpResponse& out_response, ProcessingState* state)
{
    (void)server_ip;
    (void)server_port;
    (void)state;

    Result<RequestProcessorOutput> r =
        renderer_.respond(route.getHttpStatus(), out_response);
    if (r.isError())
        return Result<HandlerResult>(ERROR, r.getErrorMessage());

    HandlerResult res;
    res.output = r.unwrap();
    return res;
}

}  // namespace server
