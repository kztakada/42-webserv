#ifndef WEBSERV_SESSION_CONTEXT_HPP_
#define WEBSERV_SESSION_CONTEXT_HPP_

#include "http/http_request.hpp"
#include "http/http_response.hpp"
#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"
#include "server/session/fd_session/http_session/body_source.hpp"
#include "server/session/fd_session/http_session/http_request_handler.hpp"
#include "server/session/io_buffer.hpp"
#include "utils/owned_ptr.hpp"

namespace server
{

class HttpResponseWriter;
class RequestRouter;
class CgiSession;
class IHttpSessionState;

struct SessionContext
{
    http::HttpRequest request;
    http::HttpResponse response;

    TcpConnectionSocketFd socket_fd;

    IoBuffer recv_buffer;
    IoBuffer send_buffer;

    utils::OwnedPtr<BodySource> body_source;
    HttpResponseWriter* response_writer;

    IHttpSessionState* current_state;
    IHttpSessionState* pending_state;

    bool is_complete;
    int redirect_count;
    bool peer_closed;
    bool should_close_connection;
    bool socket_watch_read;
    bool socket_watch_write;

    bool in_read_backpressure;
    bool in_write_backpressure;

    CgiSession* active_cgi_session;
    HttpRequestHandler request_handler;

    SessionContext(int fd, const SocketAddress& server_addr,
        const SocketAddress& client_addr, const RequestRouter& rt);
    ~SessionContext();
};

}  // namespace server

#endif