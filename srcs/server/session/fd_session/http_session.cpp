#include "server/session/fd_session/http_session.hpp"

namespace server
{

using namespace utils::result;

HttpSession::HttpSession(int fd, const SocketAddress& server_addr,
    const SocketAddress& client_addr, FdSessionController& controller,
    const RequestRouter& router)
    : FdSession(controller, kDefaultTimeoutSec),
      request_(),
      response_(),
      socket_fd_(fd, server_addr, client_addr),
      router_(router),
      handler_(request_, response_, router, socket_fd_.getServerIp(),
          socket_fd_.getServerPort(), this),
      processor_(router_, socket_fd_.getServerIp(), socket_fd_.getServerPort()),
      body_source_(NULL),
      response_writer_(NULL),
      recv_buffer_(),
      send_buffer_(),
      socket_watch_read_(false),
      socket_watch_write_(false),
      state_(RECV_REQUEST),
      redirect_count_(0),
      active_cgi_session_(NULL),
      peer_closed_(false),
      should_close_connection_(false)
{
    updateLastActiveTime();
}

HttpSession::~HttpSession()
{
    if (response_writer_ != NULL)
    {
        delete response_writer_;
        response_writer_ = NULL;
    }
    if (body_source_ != NULL)
    {
        delete body_source_;
        body_source_ = NULL;
    }
}

void HttpSession::getInitialWatchSpecs(
    std::vector<FdSession::FdWatchSpec>* out) const
{
    if (out == NULL)
        return;
    // 最初は read のみ watch し、書き込みは必要時だけ有効化する。
    out->push_back(FdSession::FdWatchSpec(socket_fd_.getFd(), true, false));
}

bool HttpSession::isComplete() const { return state_ == CLOSE_WAIT; }

}  // namespace server
