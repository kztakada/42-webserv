#include "server/session/fd_session/http_session/session_context.hpp"

#include "server/session/fd_session/http_session/body_source.hpp"
#include "server/session/fd_session/http_session/http_response_writer.hpp"
#include "server/session/fd_session/http_session/states/i_http_session_state.hpp"

namespace server
{

SessionContext::SessionContext(int fd, const SocketAddress& server_addr,
    const SocketAddress& client_addr, const RequestRouter& rt)
    : request(),
      response(),
      socket_fd(fd, server_addr, client_addr),
      recv_buffer(),
      send_buffer(),
      body_source(NULL),
      response_writer(NULL),
      current_state(NULL),
      pending_state(NULL),
      is_complete(false),
      redirect_count(0),
      peer_closed(false),
      should_close_connection(false),
      socket_watch_read(false),
      socket_watch_write(false),
      in_read_backpressure(false),
      in_write_backpressure(false),
      active_cgi_session(NULL),
      request_handler(
          request, rt, socket_fd.getServerIp(), socket_fd.getServerPort(), this)
{
}

SessionContext::~SessionContext()
{
    if (response_writer != NULL)
    {
        delete response_writer;
        response_writer = NULL;
    }
    if (current_state != NULL)
    {
        delete current_state;
        current_state = NULL;
    }
    if (pending_state != NULL)
    {
        delete pending_state;
        pending_state = NULL;
    }
}

}  // namespace server