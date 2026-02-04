#ifndef WEBSERV_TCP_LISTEN_SOCKET_FD_HPP_
#define WEBSERV_TCP_LISTEN_SOCKET_FD_HPP_

#include "server/session/fd/fd_base.hpp"
#include "server/session/fd/tcp_socket/socket_address.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;

class TcpConnectionSocketFd;

// TCPリスニングソケット（新規接続を受け付ける）
class TcpListenSocketFd : public FdBase
{
   private:
    const SocketAddress listen_addr_;

   public:
    TcpListenSocketFd(int fd, const SocketAddress& listen_addr);
    virtual ~TcpListenSocketFd();

    // bind()/listen() 済みのノンブロッキング待受けソケットを作る
    static Result<TcpListenSocketFd*> listenOn(
        const IPAddress& host_ip, const PortType& port, int backlog);

    // 接続要求を受け付ける
    Result<TcpConnectionSocketFd*> accept();

    IPAddress getListenIp() const;
    PortType getListenPort() const;

    virtual std::string getResourceType() const;

   private:
    TcpListenSocketFd();
    TcpListenSocketFd(const TcpListenSocketFd& rhs);
    TcpListenSocketFd& operator=(const TcpListenSocketFd& rhs);
};

}  // namespace server

#endif
