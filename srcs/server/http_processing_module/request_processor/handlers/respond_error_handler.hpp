#ifndef WEBSERV_RESPOND_ERROR_HANDLER_HPP_
#define WEBSERV_RESPOND_ERROR_HANDLER_HPP_

#include "server/http_processing_module/request_processor/action_handler.hpp"

namespace server
{

class ErrorPageRenderer;

class RespondErrorHandler : public ActionHandler
{
   public:
    explicit RespondErrorHandler(ErrorPageRenderer& renderer)
        : renderer_(renderer)
    {
    }

    virtual utils::result::Result<HandlerResult> handle(
        const LocationRouting& route, const IPAddress& server_ip,
        const PortType& server_port, http::HttpResponse& out_response,
        ProcessingState* state);

   private:
    ErrorPageRenderer& renderer_;
};

}  // namespace server

#endif
