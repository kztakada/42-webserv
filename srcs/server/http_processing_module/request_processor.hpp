#ifndef WEBSERV_REQUEST_PROCESSOR_HPP_
#define WEBSERV_REQUEST_PROCESSOR_HPP_

#include "http/http_request.hpp"
#include "http/http_response.hpp"
#include "server/http_processing_module/request_processor/action_handler_factory.hpp"
#include "server/http_processing_module/request_processor/autoindex_renderer.hpp"
#include "server/http_processing_module/request_processor/error_page_renderer.hpp"
#include "server/http_processing_module/request_processor/internal_redirect_resolver.hpp"
#include "server/http_processing_module/request_processor/request_processor_output.hpp"
#include "server/http_processing_module/request_processor/static_file_responder.hpp"
#include "server/http_processing_module/request_router/request_router.hpp"
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
    typedef RequestProcessorOutput Output;

    explicit RequestProcessor(const RequestRouter& router);
    ~RequestProcessor();

    Result<Output> process(const http::HttpRequest& request,
        const IPAddress& server_ip, const PortType& server_port,
        http::HttpResponse& out_response);

    // パースエラー等で「明示的にエラーステータスが決まっている」場合に
    // error_page を適用してレスポンス（body含む）を生成する。
    // - error_page が内部URIの場合は、そのURIの静的コンテンツを返しつつ
    //   ステータスは error_status のままにする。
    // - 適用できない場合はデフォルトHTMLエラーページにフォールバック。
    Result<Output> processError(const http::HttpRequest& request,
        const IPAddress& server_ip, const PortType& server_port,
        const http::HttpStatus& error_status, http::HttpResponse& out_response);

   private:
    const RequestRouter& router_;
    AutoIndexRenderer autoindex_renderer_;
    ErrorPageRenderer error_renderer_;
    InternalRedirectResolver internal_redirect_;
    StaticFileResponder file_responder_;
    ActionHandlerFactory handler_factory_;

    RequestProcessor();
    RequestProcessor(const RequestProcessor& rhs);
    RequestProcessor& operator=(const RequestProcessor& rhs);
};

}  // namespace server

#endif
