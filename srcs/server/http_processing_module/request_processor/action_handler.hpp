#ifndef WEBSERV_ACTION_HANDLER_HPP_
#define WEBSERV_ACTION_HANDLER_HPP_

#include "http/http_request.hpp"
#include "http/http_response.hpp"
#include "server/http_processing_module/request_processor/request_processor_output.hpp"
#include "server/http_processing_module/request_router/location_routing.hpp"
#include "utils/result.hpp"

namespace server
{

struct ProcessingState
{
    http::HttpRequest current;
    bool has_preserved_error_status;
    http::HttpStatus preserved_error_status;

    explicit ProcessingState(const http::HttpRequest& request)
        : current(request),
          has_preserved_error_status(false),
          preserved_error_status(http::HttpStatus::OK)
    {
    }
};

struct HandlerResult
{
    bool should_continue;
    http::HttpRequest next_request;
    RequestProcessorOutput output;

    HandlerResult() : should_continue(false), next_request(), output() {}
};

class ActionHandler
{
   public:
    virtual ~ActionHandler() {}

    virtual utils::result::Result<HandlerResult> handle(
        const LocationRouting& route, const IPAddress& server_ip,
        const PortType& server_port, http::HttpResponse& out_response,
        ProcessingState* state) = 0;
};

}  // namespace server

#endif
