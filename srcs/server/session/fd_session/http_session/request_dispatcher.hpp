#ifndef WEBSERV_REQUEST_DISPATCHER_HPP_
#define WEBSERV_REQUEST_DISPATCHER_HPP_

#include "server/session/fd_session/http_session/http_request_handler.hpp"
#include "server/session/fd_session/http_session/actions/i_request_action.hpp"
#include "http/http_request.hpp"
#include "http/http_response.hpp"

namespace server {

class RequestDispatcher {
public:
    RequestDispatcher(HttpRequest& request, HttpResponse& response,
        const RequestRouter& router, const IPAddress& server_ip,
        const PortType& server_port, const void* body_store_key);

    utils::result::Result<void> consumeFromRecvBuffer(IoBuffer& recv_buffer);
    HttpRequestHandler& handler() { return handler_; }
    const HttpRequestHandler& handler() const { return handler_; }
    
    utils::result::Result<IRequestAction*> dispatch();

private:
    HttpRequestHandler handler_;
    HttpRequest& request_;
    HttpResponse& response_;
    const RequestRouter& router_;
    IPAddress server_ip_;
    PortType server_port_;

    utils::result::Result<void> finalizeUploadStoreIfNeeded_();
};

}
#endif
