#include "server/request_processor/request_processor.hpp"

#include "http/header.hpp"

namespace server
{

RequestProcessor::RequestProcessor(const RequestRouter& router,
    const IPAddress& server_ip, const PortType& server_port)
    : router_(router), server_ip_(server_ip), server_port_(server_port)
{
}

RequestProcessor::~RequestProcessor() {}

Result<RequestProcessor::Output> RequestProcessor::process(
    const http::HttpRequest& request, http::HttpResponse& out_response)
{
    (void)request;

    // ルーティングの決定（ここでは雛形：将来 LocationRouting を参照して
    // 静的ファイル / CGI / エラー等に分岐する）
    Result<LocationRouting> route_result =
        router_.route(request, server_ip_, server_port_);
    if (route_result.isError())
        return Result<Output>(ERROR, route_result.getErrorMessage());

    // 例: 200 + ボディなし
    Result<void> s = out_response.setStatus(http::HttpStatus::OK);
    if (s.isError())
        return Result<Output>(ERROR, s.getErrorMessage());

    out_response.removeHeader(
        http::HeaderName(http::HeaderName::CONTENT_LENGTH).toString());

    Output out;
    out.body_source = NULL;
    out.should_close_connection = false;
    return out;
}

}  // namespace server
