#include "server/session/fd/tcp_socket/tcp_listen_socket_fd.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"

namespace server
{

TcpListenSocketFd::TcpListenSocketFd(int fd, const SocketAddress& listen_addr)
    : FdBase(fd), listen_addr_(listen_addr)
{
}

TcpListenSocketFd::~TcpListenSocketFd() {}

Result<TcpConnectionSocketFd*> TcpListenSocketFd::accept()
{
    struct sockaddr_in client_addr;
    while (true)
    {
        socklen_t client_len = sizeof(client_addr);
        int conn_fd =
            ::accept(fd_, (struct sockaddr*)&client_addr, &client_len);
        if (conn_fd >= 0)
        {
            SocketAddress client_socket_addr(client_addr);

            // listen_addr_ は bind() 時のアドレスなので、wildcard(0.0.0.0)
            // の場合など
            // 接続ソケットのローカル端点（実際にクライアントが接続した宛先）と一致しない。
            // conn_fd から getsockname() で正確な server_addr を作る。
            SocketAddress server_socket_addr = listen_addr_;
            struct sockaddr_in local_addr;
            socklen_t local_len = sizeof(local_addr);
            if (::getsockname(
                    conn_fd, (struct sockaddr*)&local_addr, &local_len) == 0)
            {
                server_socket_addr = SocketAddress(local_addr);
            }

            TcpConnectionSocketFd* conn_socket = new TcpConnectionSocketFd(
                conn_fd, server_socket_addr, client_socket_addr);
            return conn_socket;
        }

        if (errno == EINTR)
        {
            // シグナル割り込み。リトライする。
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // ノンブロッキング: 現在 accept できる接続が無いだけ。
            return static_cast<TcpConnectionSocketFd*>(NULL);
        }

        std::string msg = "accept() failed: ";
        msg += ::strerror(errno);
        return Result<TcpConnectionSocketFd*>(ERROR, msg);
    }
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
