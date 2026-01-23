#include "server/session/fd/tcp_socket/tcp_listen_socket_fd.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"

namespace server
{

TcpListenSocketFd::TcpListenSocketFd(int fd, const SocketAddress& listen_addr)
    : FdBase(fd), listen_addr_(listen_addr)
{
}

TcpListenSocketFd::~TcpListenSocketFd()
{
    if (fd_ >= 0)
    {
        close(fd_);
    }
}

Result<TcpConnectionSocketFd*> TcpListenSocketFd::accept()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int conn_fd = ::accept(fd_, (struct sockaddr*)&client_addr, &client_len);
    if (conn_fd < 0)
    {
        return Result<TcpConnectionSocketFd*>(ERROR, "Accept failed");
    }

    SocketAddress client_socket_addr(client_addr);
    TcpConnectionSocketFd* conn_socket =
        new TcpConnectionSocketFd(conn_fd, listen_addr_, client_socket_addr);

    return conn_socket;
}

IPAddress TcpListenSocketFd::getListenIp() const
{
    return listen_addr_.getIp();
}

PortType TcpListenSocketFd::getListenPort() const
{
    return listen_addr_.getPort();
}

std::string TcpListenSocketFd::getResourceType() const
{
    return "TcpListenSocketFd";
}

}  // namespace server
