#include "server/session/fd_session/http_session/http_handler.hpp"

#include <limits>

namespace server
{
using namespace utils::result;
using namespace http;

HttpHandler::HttpHandler(HttpRequest& request, HttpResponse& response,
    const RequestRouter& router, const IPAddress& server_ip,
    const PortType& server_port, const void* body_store_key)
    : request_(request),
      response_(response),
      router_(router),
      server_ip_(server_ip),
      server_port_(server_port),
      body_store_(body_store_key),
      body_sink_(request_, body_store_),
      has_routing_(false),
      location_routing_(),
      next_step_(NEED_MORE_DATA),
      should_close_connection_(false)
{
}

HttpHandler::~HttpHandler() {}

void HttpHandler::reset()
{
    has_routing_ = false;
    location_routing_ = LocationRouting();
    next_step_ = NEED_MORE_DATA;
    should_close_connection_ = false;
    body_store_.reset();
}

Result<void> HttpHandler::consumeFromRecvBuffer(IoBuffer& recv_buffer)
{
    while (recv_buffer.size() > 0)
    {
        if (request_.isParseComplete())
            break;

        if (!request_.isHeaderComplete())
        {
            const utils::Byte* data =
                reinterpret_cast<const utils::Byte*>(recv_buffer.data());
            const size_t len = recv_buffer.size();

            // ルーティングに応じた Limits を適用したいので、
            // ヘッダー確定時点で一旦止める。
            Result<size_t> parsed = request_.parse(data, len, NULL, true);
            if (parsed.isError())
                return Result<void>(ERROR, parsed.getErrorMessage());

            if (parsed.unwrap() == 0)
                break;
            recv_buffer.consume(parsed.unwrap());

            if (request_.isHeaderComplete())
            {
                Result<void> limit_result = ensureRoutingAndApplyBodyLimit_();
                if (limit_result.isError())
                    return limit_result;
            }
            continue;
        }

        Result<void> limit_result = ensureRoutingAndApplyBodyLimit_();
        if (limit_result.isError())
            return limit_result;

        const utils::Byte* data =
            reinterpret_cast<const utils::Byte*>(recv_buffer.data());
        const size_t len = recv_buffer.size();
        Result<size_t> parsed = request_.parse(data, len, &body_sink_, false);
        if (parsed.isError())
            return Result<void>(ERROR, parsed.getErrorMessage());

        if (parsed.unwrap() == 0)
            break;
        recv_buffer.consume(parsed.unwrap());
    }

    return Result<void>();
}

Result<void> HttpHandler::onRequestReady()
{
    if (!has_routing_)
    {
        Result<void> routing_result = decideRouting_();
        if (routing_result.isError())
            return routing_result;
    }

    next_step_ = SEND_RESPONSE;
    return Result<void>();
}

HttpHandler::ConditionalBodySink::ConditionalBodySink(
    const HttpRequest& request, BodyStore& store)
    : request_(request), store_(store)
{
}

Result<void> HttpHandler::ConditionalBodySink::write(
    const utils::Byte* data, size_t len)
{
    if (request_.getMethod() == HttpMethod::GET)
        return Result<void>();
    return store_.append(data, len);
}

Result<void> HttpHandler::decideRouting_()
{
    Result<LocationRouting> route_result =
        router_.route(request_, server_ip_, server_port_);
    if (route_result.isError())
        return Result<void>(ERROR, route_result.getErrorMessage());

    location_routing_ = route_result.unwrap();
    has_routing_ = true;
    return Result<void>();
}

Result<void> HttpHandler::ensureRoutingAndApplyBodyLimit_()
{
    if (!has_routing_)
    {
        Result<void> routing_result = decideRouting_();
        if (routing_result.isError())
            return routing_result;
    }

    Result<unsigned long> max_body_result =
        location_routing_.clientMaxBodySize();
    if (max_body_result.isError())
    {
        return Result<void>(ERROR, max_body_result.getErrorMessage());
    }

    const unsigned long max_body = max_body_result.unwrap();

    HttpRequest::Limits limits = request_.getLimits();
    if (max_body == 0)
    {
        limits.max_body_bytes = 0;
    }
    else
    {
        const size_t size_t_max = std::numeric_limits<size_t>::max();
        if (max_body > static_cast<unsigned long>(size_t_max))
            limits.max_body_bytes = size_t_max;
        else
            limits.max_body_bytes = static_cast<size_t>(max_body);
    }
    request_.setLimits(limits);

    return Result<void>();
}

}  // namespace server
