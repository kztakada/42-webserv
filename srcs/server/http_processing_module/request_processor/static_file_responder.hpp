#ifndef WEBSERV_STATIC_FILE_RESPONDER_HPP_
#define WEBSERV_STATIC_FILE_RESPONDER_HPP_

#include <sys/stat.h>

#include <string>

#include "http/http_response.hpp"
#include "server/http_processing_module/request_processor/request_processor_output.hpp"
#include "utils/result.hpp"

namespace server
{

class StaticFileResponder
{
   public:
    utils::result::Result<RequestProcessorOutput> respondFile(
        const std::string& path, const struct stat& st,
        const http::HttpStatus& status, http::HttpResponse& out_response) const;

   private:
    static std::string extractExtension_(const std::string& path);
};

}  // namespace server

#endif
