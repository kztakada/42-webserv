#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"

#include <unistd.h>

namespace server
{

TcpConnectionSocketFd::TcpConnectionSocketFd(
    int fd, const SocketAddress& server_addr, const SocketAddress& client_addr)
    : FdBase(fd),
      server_addr_(server_addr),
      client_addr_(client_addr),
      is_shutdown_(false)
{
}

TcpConnectionSocketFd::~TcpConnectionSocketFd() {}

IPAddress TcpConnectionSocketFd::getServerIp() const
{
    if (is_shutdown_)
        return IPAddress();
    return server_addr_.getIp();
}

PortType TcpConnectionSocketFd::getServerPort() const
{
    if (is_shutdown_)
        return PortType();
    return server_addr_.getPort();
}

IPAddress TcpConnectionSocketFd::getClientIp() const
{
    if (is_shutdown_)
        return IPAddress();
    return client_addr_.getIp();
}

PortType TcpConnectionSocketFd::getClientPort() const
{
    if (is_shutdown_)
        return PortType();
    return client_addr_.getPort();
}

std::string TcpConnectionSocketFd::getClientName() const
{
    if (is_shutdown_)
        return std::string();
    return client_addr_.getName();
}

const SocketAddress& TcpConnectionSocketFd::getServerAddress() const
{
    return server_addr_;
}

const SocketAddress& TcpConnectionSocketFd::getClientAddress() const
{
    return client_addr_;
}

void TcpConnectionSocketFd::shutdown()
{
    if (!is_shutdown_ && fd_ >= 0)
    {
        // ::shutdown(fd_, SHUT_RDWR);
        is_shutdown_ = true;
    }
}

bool TcpConnectionSocketFd::isShutdown() const { return is_shutdown_; }

std::string TcpConnectionSocketFd::getResourceType() const
{
    return "TcpConnectionSocketFd";
}

}  // namespace server
