#ifndef WEBSERV_REQUEST_PROCESSOR_OUTPUT_HPP_
#define WEBSERV_REQUEST_PROCESSOR_OUTPUT_HPP_

namespace server
{

class BodySource;

struct RequestProcessorOutput
{
    BodySource* body_source;       // 呼び出し側が delete
    bool should_close_connection;  // HTTP/1.0 等で close-delimited
                                   // が必要な場合

    RequestProcessorOutput() : body_source(NULL), should_close_connection(false)
    {
    }
};

}  // namespace server

#endif
