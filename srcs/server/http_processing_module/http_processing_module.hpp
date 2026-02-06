#ifndef WEBSERV_HTTP_PROCESSING_MODULE_HPP_
#define WEBSERV_HTTP_PROCESSING_MODULE_HPP_

#include "server/config/server_config.hpp"
#include "server/http_processing_module/request_dispatcher.hpp"
#include "server/http_processing_module/request_processor.hpp"
#include "server/http_processing_module/request_router/request_router.hpp"
#include "server/http_processing_module/session_cgi_handler.hpp"

namespace server
{

struct HttpProcessingModule
{
    RequestRouter router;
    SessionCgiHandler cgi_handler;
    RequestDispatcher dispatcher;
    RequestProcessor processor;

    explicit HttpProcessingModule(const ServerConfig& config)
        : router(config), cgi_handler(), dispatcher(), processor(router)
    {
    }

   private:
    HttpProcessingModule();
};

}  // namespace server

#endif
