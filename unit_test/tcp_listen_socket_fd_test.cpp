#include "server/session/fd/tcp_socket/tcp_listen_socket_fd.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "server/session/fd/tcp_socket/socket_address.hpp"
#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"

static int createListenSocketOrDie_(struct sockaddr_in* out_addr)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(fd, 0);
    if (fd < 0)
    {
        return -1;
    }

    int yes = 1;
    EXPECT_EQ(::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)), 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);  // ephemeral

    EXPECT_EQ(::bind(fd, (struct sockaddr*)&addr, sizeof(addr)), 0);
    EXPECT_EQ(::listen(fd, 16), 0);

    socklen_t len = sizeof(addr);
    EXPECT_EQ(::getsockname(fd, (struct sockaddr*)&addr, &len), 0);

    if (out_addr)
    {
        *out_addr = addr;
    }
    return fd;
}

static int createListenSocketOnAnyOrDie_(struct sockaddr_in* out_addr)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(fd, 0);
    if (fd < 0)
    {
        return -1;
    }

    int yes = 1;
    EXPECT_EQ(::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)), 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);  // ephemeral

    EXPECT_EQ(::bind(fd, (struct sockaddr*)&addr, sizeof(addr)), 0);
    EXPECT_EQ(::listen(fd, 16), 0);

    socklen_t len = sizeof(addr);
    EXPECT_EQ(::getsockname(fd, (struct sockaddr*)&addr, &len), 0);

    if (out_addr)
    {
        *out_addr = addr;
    }
    return fd;
}

static void setNonBlockingOrDie_(int fd)
{
    int flags = ::fcntl(fd, F_GETFL, 0);
    ASSERT_NE(flags, -1);
    ASSERT_NE(::fcntl(fd, F_SETFL, flags | O_NONBLOCK), -1);
}

TEST(TcpListenSocketFd, AcceptReturnsNullWhenNoPendingConnections)
{
    struct sockaddr_in listen_addr;
    int listen_fd_raw = createListenSocketOrDie_(&listen_addr);
    ASSERT_GE(listen_fd_raw, 0);

    setNonBlockingOrDie_(listen_fd_raw);

    server::SocketAddress listen_sa(listen_addr);
    server::TcpListenSocketFd listen_fd(listen_fd_raw, listen_sa);

    utils::result::Result<server::TcpConnectionSocketFd*> r =
        listen_fd.accept();
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.unwrap(), static_cast<server::TcpConnectionSocketFd*>(NULL));
}

TEST(TcpListenSocketFd, AcceptReturnsConnectionWhenClientConnects)
{
    struct sockaddr_in listen_addr;
    int listen_fd_raw = createListenSocketOrDie_(&listen_addr);
    ASSERT_GE(listen_fd_raw, 0);

    // client connects first, then accept() should return immediately.
    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client_fd, 0);

    int rc = ::connect(
        client_fd, (struct sockaddr*)&listen_addr, sizeof(listen_addr));
    ASSERT_EQ(rc, 0);

    server::SocketAddress listen_sa(listen_addr);
    server::TcpListenSocketFd listen_fd(listen_fd_raw, listen_sa);

    utils::result::Result<server::TcpConnectionSocketFd*> r =
        listen_fd.accept();
    ASSERT_TRUE(r.isOk());

    server::TcpConnectionSocketFd* conn = r.unwrap();
    ASSERT_NE(conn, static_cast<server::TcpConnectionSocketFd*>(NULL));

    delete conn;
    ::close(client_fd);
}

TEST(TcpListenSocketFd, AcceptUsesActualLocalEndpointWhenListenIsWildcard)
{
    struct sockaddr_in listen_addr_any;
    int listen_fd_raw = createListenSocketOnAnyOrDie_(&listen_addr_any);
    ASSERT_GE(listen_fd_raw, 0);

    // connect() 先は 127.0.0.1:port にする（listen が 0.0.0.0
    // でも接続先IPは具体値）
    struct sockaddr_in connect_addr = listen_addr_any;
    connect_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client_fd, 0);

    int rc = ::connect(
        client_fd, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
    ASSERT_EQ(rc, 0);

    server::SocketAddress listen_sa(listen_addr_any);
    server::TcpListenSocketFd listen_fd(listen_fd_raw, listen_sa);

    utils::result::Result<server::TcpConnectionSocketFd*> r =
        listen_fd.accept();
    ASSERT_TRUE(r.isOk());

    server::TcpConnectionSocketFd* conn = r.unwrap();
    ASSERT_NE(conn, static_cast<server::TcpConnectionSocketFd*>(NULL));

    EXPECT_EQ(conn->getServerIp().toString(), std::string("127.0.0.1"));
    EXPECT_EQ(conn->getServerPort().toString(),
        server::SocketAddress(connect_addr).getPort().toString());

    delete conn;
    ::close(client_fd);
}
