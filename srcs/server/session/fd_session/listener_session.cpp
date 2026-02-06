#include "server/session/fd_session/listener_session.hpp"

#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "utils/log.hpp"

namespace server
{

ListenerSession::ListenerSession(TcpListenSocketFd* listen_fd,
    FdSessionController& controller, HttpProcessingModule& module)
    : FdSession(controller, kNoTimeoutSeconds),
      listen_fd_(listen_fd),
      module_(module)
{
    updateLastActiveTime();
}

ListenerSession::~ListenerSession()
{
    if (listen_fd_ != NULL)
    {
        delete listen_fd_;
        listen_fd_ = NULL;
    }
}

bool ListenerSession::isTimedOut() const { return false; }

bool ListenerSession::isComplete() const { return false; }

void ListenerSession::getInitialWatchSpecs(
    std::vector<FdSession::FdWatchSpec>* out) const
{
    if (out == NULL)
        return;
    if (listen_fd_ == NULL)
        return;
    out->push_back(FdSession::FdWatchSpec(listen_fd_->getFd(), true, false));
}

Result<void> ListenerSession::handleEvent(const FdEvent& event)
{
    updateLastActiveTime();
    if (event.type != kReadEvent)
        return Result<void>();

    acceptNewConnection();
    return Result<void>();
}

void ListenerSession::acceptNewConnection()
{
    if (listen_fd_ == NULL)
        return;
    for (;;)
    {
        Result<TcpConnectionSocketFd*> accept_result = listen_fd_->accept();
        if (accept_result.isError())
        {
            utils::Log::error("ListenerSession accept() failed: ",
                accept_result.getErrorMessage());
            return;
        }

        TcpConnectionSocketFd* conn = accept_result.unwrap();
        if (conn == NULL)
            return;

        const SocketAddress server_addr = conn->getServerAddress();
        const SocketAddress client_addr = conn->getClientAddress();
        const int fd = conn->release();
        delete conn;

        if (fd < 0)
            continue;

        FdSession* s =
            new HttpSession(fd, server_addr, client_addr, controller_, module_);
        Result<void> d = controller_.delegateSession(s);
        if (d.isError())
        {
            utils::Log::error(
                "delegateSession(HttpSession) failed: ", d.getErrorMessage());
            delete s;
        }
    }
}

}  // namespace server
