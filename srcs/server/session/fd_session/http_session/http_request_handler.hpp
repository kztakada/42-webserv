#ifndef WEBSERV_HTTP_REQUEST_HANDLER_HPP_
#define WEBSERV_HTTP_REQUEST_HANDLER_HPP_

#include "http/http_request.hpp"
#include "http/http_response.hpp"
#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/http_processing_module/request_router/location_routing.hpp"
#include "server/http_processing_module/request_router/request_router.hpp"
#include "server/session/fd_session/http_session/body_store.hpp"
#include "server/session/io_buffer.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;
using namespace http;

class HttpRequestHandler
{
   public:
    enum NextStep
    {
        NEED_MORE_DATA,
        EXECUTE_CGI,
        SEND_RESPONSE,
        CLOSE_CONNECTION
    };

    HttpRequestHandler(HttpRequest& request, HttpResponse& response,
        const RequestRouter& router, const IPAddress& server_ip,
        const PortType& server_port, const void* body_store_key);

    ~HttpRequestHandler();

    void reset();

    Result<void> consumeFromRecvBuffer(IoBuffer& recv_buffer);

    BodyStore& bodyStore() { return body_store_; }
    const BodyStore& bodyStore() const { return body_store_; }

    Result<void> onRequestReady();

    bool hasLocationRouting() const { return has_routing_; }
    const LocationRouting& getLocationRouting() const
    {
        return location_routing_;
    }

    NextStep getNextStep() const { return next_step_; }
    bool shouldCloseConnection() const { return should_close_connection_; }

   private:
    HttpRequest& request_;
    HttpResponse& response_;
    const RequestRouter& router_;
    IPAddress server_ip_;
    PortType server_port_;

    class ConditionalBodySink : public HttpRequest::BodySink
    {
       public:
        ConditionalBodySink(const HttpRequest& request, BodyStore& store);

        virtual Result<void> write(const utils::Byte* data, size_t len);

       private:
        const HttpRequest& request_;
        BodyStore& store_;

        ConditionalBodySink();
        ConditionalBodySink(const ConditionalBodySink& rhs);
        ConditionalBodySink& operator=(const ConditionalBodySink& rhs);
    };

    BodyStore body_store_;
    ConditionalBodySink body_sink_;

    bool has_routing_;
    LocationRouting location_routing_;

    bool has_configured_body_store_for_upload_;

    NextStep next_step_;
    bool should_close_connection_;

    HttpRequestHandler();
    HttpRequestHandler(const HttpRequestHandler& rhs);
    HttpRequestHandler& operator=(const HttpRequestHandler& rhs);

    Result<void> decideRouting_();
    Result<void> ensureRoutingAndApplyBodyLimit_();
};

}  // namespace server

#endif