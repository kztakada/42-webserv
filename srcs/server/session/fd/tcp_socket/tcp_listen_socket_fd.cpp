#include "server/session/fd/tcp_socket/tcp_listen_socket_fd.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"

namespace server
{

static Result<uint32_t> parseIpv4ToNetwork_(const std::string& ip_str)
{
    unsigned int octets[4];
    unsigned int octet_index = 0;
    unsigned int current = 0;
    bool has_digit = false;
    unsigned int digit_count = 0;

    for (size_t i = 0; i < ip_str.size(); ++i)
    {
        const char c = ip_str[i];
        if (c >= '0' && c <= '9')
        {
            has_digit = true;
            ++digit_count;
            if (digit_count > 3)
                return Result<uint32_t>(ERROR, "invalid ip");
            current = current * 10u + static_cast<unsigned int>(c - '0');
            if (current > 255u)
                return Result<uint32_t>(ERROR, "invalid ip");
        }
        else if (c == '.')
        {
            if (!has_digit)
                return Result<uint32_t>(ERROR, "invalid ip");
            if (octet_index >= 4)
                return Result<uint32_t>(ERROR, "invalid ip");
            octets[octet_index] = current;
            ++octet_index;
            current = 0;
            has_digit = false;
            digit_count = 0;
        }
        else
        {
            return Result<uint32_t>(ERROR, "invalid ip");
        }
    }

    if (!has_digit)
        return Result<uint32_t>(ERROR, "invalid ip");
    if (octet_index != 3)
        return Result<uint32_t>(ERROR, "invalid ip");
    octets[3] = current;

    const uint32_t host_order =
        (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | (octets[3]);
    return htonl(host_order);
}

static Result<void> setNonBlocking_(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return Result<void>(ERROR, "fcntl(F_GETFL) failed");
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return Result<void>(ERROR, "fcntl(F_SETFL) failed");
    return Result<void>();
}

TcpListenSocketFd::TcpListenSocketFd(int fd, const SocketAddress& listen_addr)
    : FdBase(fd), listen_addr_(listen_addr)
{
}

TcpListenSocketFd::~TcpListenSocketFd() {}

Result<TcpListenSocketFd*> TcpListenSocketFd::listenOn(
    const IPAddress& host_ip, const PortType& port, int backlog)
{
    const unsigned int port_number = port.toNumber();
    if (port_number == 0)
        return Result<TcpListenSocketFd*>(ERROR, "listen port is invalid");

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return Result<TcpListenSocketFd*>(ERROR, "socket() failed");

    int yes = 1;
    (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_number));

    if (host_ip.isWildcard())
    {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else
    {
        Result<uint32_t> saddr = parseIpv4ToNetwork_(host_ip.toString());
        if (saddr.isError())
        {
            ::close(fd);
            return Result<TcpListenSocketFd*>(ERROR, "listen ip is invalid");
        }
        addr.sin_addr.s_addr = saddr.unwrap();
    }

    if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(fd);
        return Result<TcpListenSocketFd*>(ERROR, "bind() failed");
    }
    if (::listen(fd, backlog) < 0)
    {
        ::close(fd);
        return Result<TcpListenSocketFd*>(ERROR, "listen() failed");
    }

    Result<void> nb = setNonBlocking_(fd);
    if (nb.isError())
    {
        ::close(fd);
        return Result<TcpListenSocketFd*>(ERROR, nb.getErrorMessage());
    }

    struct sockaddr_in bound;
    std::memset(&bound, 0, sizeof(bound));
    socklen_t len = sizeof(bound);
    if (::getsockname(fd, (struct sockaddr*)&bound, &len) != 0)
    {
        ::close(fd);
        return Result<TcpListenSocketFd*>(ERROR, "getsockname() failed");
    }

    return new TcpListenSocketFd(fd, SocketAddress(bound));
}

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
