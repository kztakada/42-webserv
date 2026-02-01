#ifndef WEBSERV_TCP_CONNECTION_SOCKET_FD_HPP_
#define WEBSERV_TCP_CONNECTION_SOCKET_FD_HPP_

#include <string>

#include "server/session/fd/fd_base.hpp"
#include "server/session/fd/tcp_socket/socket_address.hpp"

namespace server
{

// TCP接続済みソケット（クライアントとの通信）
class TcpConnectionSocketFd : public FdBase
{
   private:
    const SocketAddress server_addr_;
    const SocketAddress client_addr_;
    bool is_shutdown_;

   public:
    TcpConnectionSocketFd(int fd, const SocketAddress& server_addr,
        const SocketAddress& client_addr);
    virtual ~TcpConnectionSocketFd();

    IPAddress getServerIp() const;
    PortType getServerPort() const;
    IPAddress getClientIp() const;
    PortType getClientPort() const;
    std::string getClientName() const;

    void shutdown();
    bool isShutdown() const;
    virtual std::string getResourceType() const;

   private:
    TcpConnectionSocketFd();
    TcpConnectionSocketFd(const TcpConnectionSocketFd& rhs);
    TcpConnectionSocketFd& operator=(const TcpConnectionSocketFd& rhs);
};

}  // namespace server

#endif
