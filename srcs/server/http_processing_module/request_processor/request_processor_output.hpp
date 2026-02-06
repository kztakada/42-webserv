#ifndef WEBSERV_REQUEST_PROCESSOR_OUTPUT_HPP_
#define WEBSERV_REQUEST_PROCESSOR_OUTPUT_HPP_

#include "server/session/fd_session/http_session/body_source.hpp"
#include "utils/owned_ptr.hpp"

namespace server
{

struct RequestProcessorOutput
{
    // 所有権はこの Output が持つ（RAII）。
    // HttpSession 等へ渡す場合は release() で移譲する。
    utils::OwnedPtr<BodySource> body_source;
    bool should_close_connection;  // HTTP/1.0 等で close-delimited
                                   // が必要な場合

    RequestProcessorOutput() : body_source(NULL), should_close_connection(false)
    {
    }
};

}  // namespace server

#endif
