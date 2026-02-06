#include "server/http_processing_module/request_processor/handlers/redirect_external_handler.hpp"

namespace server
{

using utils::result::Result;

Result<HandlerResult> RedirectExternalHandler::handle(
    const LocationRouting& route, const IPAddress& server_ip,
    const PortType& server_port, http::HttpResponse& out_response,
    ProcessingState* state)
{
    (void)server_ip;
    (void)server_port;
    (void)state;

    HandlerResult res;

    Result<void> s = out_response.setStatus(route.getHttpStatus());
    if (s.isError())
        return Result<HandlerResult>(ERROR, s.getErrorMessage());

    Result<std::string> loc = route.getRedirectLocation();
    if (loc.isError())
        return Result<HandlerResult>(ERROR, loc.getErrorMessage());

    Result<void> h = out_response.setHeader("Location", loc.unwrap());
    if (h.isError())
        return Result<HandlerResult>(ERROR, h.getErrorMessage());

    (void)out_response.setExpectedContentLength(0);
    res.output.body_source = NULL;
    res.output.should_close_connection = false;
    return res;
}

}  // namespace server
