#ifndef WEBSERV_HTTP_PROCESSING_MODULE_HPP_
#define WEBSERV_HTTP_PROCESSING_MODULE_HPP_

#include "http/http_response_encoder.hpp"
#include "server/config/server_config.hpp"
#include "server/http_processing_module/request_dispatcher.hpp"
#include "server/http_processing_module/request_processor.hpp"
#include "server/http_processing_module/request_router/request_router.hpp"
#include "server/http_processing_module/session_cgi_handler.hpp"
#include "server/session/fd_session_controller.hpp"
#include "utils/result.hpp"

namespace server
{

struct SessionContext;

struct HttpProcessingModule
{
    RequestRouter router;
    SessionCgiHandler cgi_handler;
    RequestDispatcher dispatcher;
    RequestProcessor processor;

    explicit HttpProcessingModule(
        const ServerConfig& config, FdSessionController& controller)
        : router(config),
          cgi_handler(controller),
          dispatcher(router),
          processor(router)
    {
    }

    http::HttpResponseEncoder::Options makeEncoderOptions(
        const http::HttpRequest& request);
    utils::result::Result<void> setSimpleErrorResponse(
        http::HttpResponse& response, http::HttpStatus status);
    utils::result::Result<void> buildErrorOutput(SessionContext& context,
        http::HttpStatus status, RequestProcessor::Output* out);
    utils::result::Result<void> buildProcessorOutputOrServerError(
        SessionContext& context, RequestProcessor::Output* out);

   private:
    HttpProcessingModule();
};

}  // namespace server

#endif
