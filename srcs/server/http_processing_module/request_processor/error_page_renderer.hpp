#ifndef WEBSERV_ERROR_PAGE_RENDERER_HPP_
#define WEBSERV_ERROR_PAGE_RENDERER_HPP_

#include <string>

#include "http/http_response.hpp"
#include "server/http_processing_module/request_processor/request_processor_output.hpp"
#include "utils/result.hpp"

namespace server
{

class ErrorPageRenderer
{
   public:
    utils::result::Result<RequestProcessorOutput> respond(
        const http::HttpStatus& status, http::HttpResponse& out_response) const;

   private:
    std::string buildDefaultErrorPageBody_(
        const http::HttpStatus& status) const;
    static std::string htmlEscape_(const std::string& s);
};

}  // namespace server

#endif
