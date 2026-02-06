#ifndef WEBSERV_INTERNAL_REDIRECT_HANDLER_HPP_
#define WEBSERV_INTERNAL_REDIRECT_HANDLER_HPP_

#include "server/http_processing_module/request_processor/action_handler.hpp"

namespace server
{

class InternalRedirectHandler : public ActionHandler
{
   public:
    virtual utils::result::Result<HandlerResult> handle(
        const LocationRouting& route, const IPAddress& server_ip,
        const PortType& server_port, http::HttpResponse& out_response,
        ProcessingState* state);
};

}  // namespace server

#endif
