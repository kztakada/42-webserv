#include "server/reactor/fd_event_reactor/epoll_reactor.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

namespace
{
class ScopedFd
{
   public:
    explicit ScopedFd(int fd) : fd_(fd) {}
    ~ScopedFd()
    {
        if (fd_ >= 0)
            close(fd_);
    }

    int get() const { return fd_; }

    int release()
    {
        int tmp = fd_;
        fd_ = -1;
        return tmp;
    }

   private:
    int fd_;

    ScopedFd(const ScopedFd&);
    ScopedFd& operator=(const ScopedFd&);
};

static bool containsEvent(const std::vector<server::FdEvent>& events, int fd,
    server::FdEventType type)
{
    for (size_t i = 0; i < events.size(); ++i)
    {
        if (events[i].fd == fd && events[i].type == type)
            return true;
    }
    return false;
}

static bool setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}
}  // namespace

TEST(EpollFdEventReactor, ReturnsReadEventWhenPipeReadable)
{
    int fds[2];
    ASSERT_EQ(0, pipe(fds));
    ScopedFd read_fd(fds[0]);
    ScopedFd write_fd(fds[1]);
	ASSERT_TRUE(setNonBlocking(read_fd.get()));
	ASSERT_TRUE(setNonBlocking(write_fd.get()));

    server::EpollFdEventReactor reactor;

    server::FdEvent watch;
    watch.fd = read_fd.get();
    watch.type = server::kReadEvent;
    watch.session = NULL;
    watch.is_opposite_close = false;

    utils::result::Result<void> add_r = reactor.addWatch(watch);
    ASSERT_TRUE(add_r.isOk());

    const char b = 'x';
    ASSERT_EQ(1, write(write_fd.get(), &b, 1));

    utils::result::Result<const std::vector<server::FdEvent>&> wait_r =
        reactor.waitEvents(100);
    ASSERT_TRUE(wait_r.isOk());

    const std::vector<server::FdEvent>& events = wait_r.unwrap();
    EXPECT_TRUE(containsEvent(events, read_fd.get(), server::kReadEvent));
}

TEST(EpollFdEventReactor, ReturnsWriteEventWhenPipeWritable)
{
    int fds[2];
    ASSERT_EQ(0, pipe(fds));
    ScopedFd read_fd(fds[0]);
    ScopedFd write_fd(fds[1]);
	ASSERT_TRUE(setNonBlocking(read_fd.get()));
	ASSERT_TRUE(setNonBlocking(write_fd.get()));

    server::EpollFdEventReactor reactor;

    server::FdEvent watch;
    watch.fd = write_fd.get();
    watch.type = server::kWriteEvent;
    watch.session = NULL;
    watch.is_opposite_close = false;

    utils::result::Result<void> add_r = reactor.addWatch(watch);
    ASSERT_TRUE(add_r.isOk());

    utils::result::Result<const std::vector<server::FdEvent>&> wait_r =
        reactor.waitEvents(0);
    ASSERT_TRUE(wait_r.isOk());

    const std::vector<server::FdEvent>& events = wait_r.unwrap();
    EXPECT_TRUE(containsEvent(events, write_fd.get(), server::kWriteEvent));
}

TEST(EpollFdEventReactor, RemoveWatchStopsReceivingEvents)
{
    int fds[2];
    ASSERT_EQ(0, pipe(fds));
    ScopedFd read_fd(fds[0]);
    ScopedFd write_fd(fds[1]);
	ASSERT_TRUE(setNonBlocking(read_fd.get()));
	ASSERT_TRUE(setNonBlocking(write_fd.get()));

    server::EpollFdEventReactor reactor;

    server::FdEvent add;
    add.fd = read_fd.get();
    add.type = server::kReadEvent;
    add.session = NULL;
    add.is_opposite_close = false;

    ASSERT_TRUE(reactor.addWatch(add).isOk());

    server::FdEvent remove;
    remove.fd = read_fd.get();
    remove.type = server::kReadEvent;
    remove.session = NULL;
    remove.is_opposite_close = false;

    ASSERT_TRUE(reactor.removeWatch(remove).isOk());

    const char b = 'x';
    ASSERT_EQ(1, write(write_fd.get(), &b, 1));

    utils::result::Result<const std::vector<server::FdEvent>&> wait_r =
        reactor.waitEvents(20);
    ASSERT_TRUE(wait_r.isOk());
    EXPECT_EQ(0u, wait_r.unwrap().size());
}

TEST(EpollFdEventReactor, DeleteWatchStopsReceivingEvents)
{
    int fds[2];
    ASSERT_EQ(0, pipe(fds));
    ScopedFd read_fd(fds[0]);
    ScopedFd write_fd(fds[1]);
	ASSERT_TRUE(setNonBlocking(read_fd.get()));
	ASSERT_TRUE(setNonBlocking(write_fd.get()));

    server::EpollFdEventReactor reactor;

    server::FdEvent add;
    add.fd = read_fd.get();
    add.type = server::kReadEvent;
    add.session = NULL;
    add.is_opposite_close = false;

    ASSERT_TRUE(reactor.addWatch(add).isOk());
    ASSERT_TRUE(reactor.deleteWatch(read_fd.get()).isOk());

    const char b = 'x';
    ASSERT_EQ(1, write(write_fd.get(), &b, 1));

    utils::result::Result<const std::vector<server::FdEvent>&> wait_r =
        reactor.waitEvents(20);
    ASSERT_TRUE(wait_r.isOk());
    EXPECT_EQ(0u, wait_r.unwrap().size());
}

TEST(EpollFdEventReactor, ReportsOppositeCloseOnPeerClose)
{
    int fds[2];
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    ScopedFd a(fds[0]);
    ScopedFd b(fds[1]);
	ASSERT_TRUE(setNonBlocking(a.get()));
	ASSERT_TRUE(setNonBlocking(b.get()));

    server::EpollFdEventReactor reactor;

    server::FdEvent watch;
    watch.fd = a.get();
    watch.type = server::kReadEvent;
    watch.session = NULL;
    watch.is_opposite_close = false;

    ASSERT_TRUE(reactor.addWatch(watch).isOk());

    // peer close
    ASSERT_EQ(0, close(b.release()));

    utils::result::Result<const std::vector<server::FdEvent>&> wait_r =
        reactor.waitEvents(100);
    ASSERT_TRUE(wait_r.isOk());

    const std::vector<server::FdEvent>& events = wait_r.unwrap();
    bool found = false;
    for (size_t i = 0; i < events.size(); ++i)
    {
        if (events[i].fd == a.get() && events[i].type == server::kReadEvent)
        {
            found = true;
            EXPECT_TRUE(events[i].is_opposite_close);
        }
    }
    EXPECT_TRUE(found);
}

TEST(EpollFdEventReactor, RejectsInvalidWatchEvents)
{
    int fds[2];
    ASSERT_EQ(0, pipe(fds));
    ScopedFd read_fd(fds[0]);
    ScopedFd write_fd(fds[1]);
	ASSERT_TRUE(setNonBlocking(read_fd.get()));
	ASSERT_TRUE(setNonBlocking(write_fd.get()));

    server::EpollFdEventReactor reactor;

    server::FdEvent bad;
    bad.fd = read_fd.get();
    bad.type = server::kErrorEvent;
    bad.session = NULL;
    bad.is_opposite_close = false;

    utils::result::Result<void> r = reactor.addWatch(bad);
    EXPECT_TRUE(r.isError());
}

TEST(EpollFdEventReactor, RemoveDeleteReturnErrorForUnknownFd)
{
    server::EpollFdEventReactor reactor;

    server::FdEvent remove;
    remove.fd = 999999;
    remove.type = server::kReadEvent;
    remove.session = NULL;
    remove.is_opposite_close = false;

    EXPECT_TRUE(reactor.removeWatch(remove).isError());
    EXPECT_TRUE(reactor.deleteWatch(999999).isError());
}

TEST(EpollFdEventReactor, ReturnsErrorEventOnlyWhenConnectFails)
{
    // 1) Pick a local port that is very likely closed: bind+listen to an ephemeral port,
    // then close it so there is no listener.
    int listen_fd_raw = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listen_fd_raw, 0);
    ScopedFd listen_fd(listen_fd_raw);

    int yes = 1;
    ASSERT_EQ(0, setsockopt(listen_fd.get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));

    sockaddr_in listen_addr;
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    listen_addr.sin_port = htons(0);
    ASSERT_EQ(0, bind(listen_fd.get(), reinterpret_cast<sockaddr*>(&listen_addr),
                  sizeof(listen_addr)));
    ASSERT_EQ(0, listen(listen_fd.get(), 1));

    sockaddr_in bound_addr;
    socklen_t bound_len = sizeof(bound_addr);
    ASSERT_EQ(0, getsockname(listen_fd.get(), reinterpret_cast<sockaddr*>(&bound_addr),
                  &bound_len));
    const uint16_t port = ntohs(bound_addr.sin_port);
    ASSERT_NE(0, port);

    ASSERT_EQ(0, close(listen_fd.release()));

    // 2) Create a non-blocking client socket and watch it for "write" to learn connect result.
    int client_fd_raw = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client_fd_raw, 0);
    ScopedFd client_fd(client_fd_raw);

    int flags = fcntl(client_fd.get(), F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_NE(-1, fcntl(client_fd.get(), F_SETFL, flags | O_NONBLOCK));

    server::EpollFdEventReactor reactor;
    server::FdEvent watch;
    watch.fd = client_fd.get();
    watch.type = server::kWriteEvent;
    watch.session = NULL;
    watch.is_opposite_close = false;
    ASSERT_TRUE(reactor.addWatch(watch).isOk());

    // 3) Non-blocking connect to the now-closed port.
    sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dest.sin_port = htons(port);

    int c = connect(client_fd.get(), reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    if (c != 0)
    {
        ASSERT_TRUE(errno == EINPROGRESS || errno == ECONNREFUSED);
    }

    // 4) Expect EPOLLERR to be surfaced as kErrorEvent (and to suppress kWriteEvent).
    utils::result::Result<const std::vector<server::FdEvent>&> wait_r =
        reactor.waitEvents(200);
    ASSERT_TRUE(wait_r.isOk());

    const std::vector<server::FdEvent>& events = wait_r.unwrap();
    EXPECT_TRUE(containsEvent(events, client_fd.get(), server::kErrorEvent));
    EXPECT_FALSE(containsEvent(events, client_fd.get(), server::kWriteEvent));

    int so_error = 0;
    socklen_t so_error_len = sizeof(so_error);
    ASSERT_EQ(0,
        getsockopt(client_fd.get(), SOL_SOCKET, SO_ERROR, &so_error, &so_error_len));
    EXPECT_NE(0, so_error);
}
