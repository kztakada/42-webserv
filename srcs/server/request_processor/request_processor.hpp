#ifndef WEBSERV_REQUEST_PROCESSOR_HPP_
#define WEBSERV_REQUEST_PROCESSOR_HPP_

#include "http/http_request.hpp"
#include "http/http_response.hpp"
#include "server/request_router/request_router.hpp"
#include "server/session/fd_session/http_session/body_source.hpp"
#include "utils/result.hpp"

namespace server
{

using namespace utils::result;

// Request -> Response 生成（アプリ層の中心）
// - ここでは「何を返すか」を決める（status/headers/body source）
// - 実際のwireエンコードは HttpResponseWriter/Encoder が行う
class RequestProcessor
{
   public:
    struct Output
    {
        BodySource* body_source;       // 呼び出し側が delete
        bool should_close_connection;  // HTTP/1.0 等で close-delimited
                                       // が必要な場合

        Output() : body_source(NULL), should_close_connection(false) {}
    };

    RequestProcessor(const RequestRouter& router, const IPAddress& server_ip,
        const PortType& server_port);
    ~RequestProcessor();

    Result<Output> process(
        const http::HttpRequest& request, http::HttpResponse& out_response);

   private:
    const RequestRouter& router_;
    IPAddress server_ip_;
    PortType server_port_;

    RequestProcessor();
    RequestProcessor(const RequestProcessor& rhs);
    RequestProcessor& operator=(const RequestProcessor& rhs);
};

}  // namespace server

#endif
