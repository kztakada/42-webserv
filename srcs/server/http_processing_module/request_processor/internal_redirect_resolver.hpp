#ifndef WEBSERV_INTERNAL_REDIRECT_RESOLVER_HPP_
#define WEBSERV_INTERNAL_REDIRECT_RESOLVER_HPP_

#include <string>
#include <vector>

#include "http/http_request.hpp"
#include "server/http_processing_module/request_router/request_router.hpp"
#include "utils/result.hpp"

namespace server
{

class InternalRedirectResolver
{
   public:
    utils::result::Result<http::HttpRequest> buildInternalGetRequest(
        const std::string& uri_path, const http::HttpRequest& base) const;

    bool tryBuildErrorPageInternalRedirect(const RequestRouter& router,
        const IPAddress& server_ip, const PortType& server_port,
        const http::HttpRequest& base_request,
        const http::HttpStatus& error_status,
        http::HttpRequest* out_next) const;
};

}  // namespace server

#endif
