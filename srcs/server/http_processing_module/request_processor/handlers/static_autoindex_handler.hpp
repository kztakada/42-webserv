#ifndef WEBSERV_STATIC_AUTOINDEX_HANDLER_HPP_
#define WEBSERV_STATIC_AUTOINDEX_HANDLER_HPP_

#include "server/http_processing_module/request_processor/action_handler.hpp"

namespace server
{

class AutoIndexRenderer;
class ErrorPageRenderer;
class InternalRedirectResolver;
class StaticFileResponder;
class RequestRouter;

class StaticAutoIndexHandler : public ActionHandler
{
   public:
    StaticAutoIndexHandler(const RequestRouter& router,
        AutoIndexRenderer& autoindex_renderer,
        ErrorPageRenderer& error_renderer,
        InternalRedirectResolver& internal_redirect,
        StaticFileResponder& file_responder)
        : router_(router),
          autoindex_renderer_(autoindex_renderer),
          error_renderer_(error_renderer),
          internal_redirect_(internal_redirect),
          file_responder_(file_responder)
    {
    }

    virtual utils::result::Result<HandlerResult> handle(
        const LocationRouting& route, const IPAddress& server_ip,
        const PortType& server_port, http::HttpResponse& out_response,
        ProcessingState* state);

   private:
    const RequestRouter& router_;
    AutoIndexRenderer& autoindex_renderer_;
    ErrorPageRenderer& error_renderer_;
    InternalRedirectResolver& internal_redirect_;
    StaticFileResponder& file_responder_;
};

}  // namespace server

#endif
