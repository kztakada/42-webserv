#include "server/reactor/fd_event_reactor_factory.hpp"

#include <gtest/gtest.h>

#ifdef __linux__
#include "server/reactor/fd_event_reactor/epoll_reactor.hpp"
#endif

#include "server/reactor/fd_event_reactor/select_reactor.hpp"

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
#include "server/reactor/fd_event_reactor/kqueue_reactor.hpp"
#endif

namespace
{

TEST(FdEventReactorFactory, CreateReturnsBestImplementationForPlatform)
{
    server::FdEventReactor* reactor = server::FdEventReactorFactory::create();
    ASSERT_NE(static_cast<server::FdEventReactor*>(NULL), reactor);

#ifdef __linux__
    EXPECT_NE(static_cast<server::EpollFdEventReactor*>(NULL),
        dynamic_cast<server::EpollFdEventReactor*>(reactor));
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
    EXPECT_NE(static_cast<server::KqueueFdEventReactor*>(NULL),
        dynamic_cast<server::KqueueFdEventReactor*>(reactor));
#else
    EXPECT_NE(static_cast<server::SelectFdEventReactor*>(NULL),
        dynamic_cast<server::SelectFdEventReactor*>(reactor));
#endif

    delete reactor;
}

TEST(FdEventReactorFactory, CreateSelectAlwaysReturnsSelect)
{
    server::FdEventReactor* reactor =
        server::FdEventReactorFactory::createSelect();
    ASSERT_NE(static_cast<server::FdEventReactor*>(NULL), reactor);
    EXPECT_NE(static_cast<server::SelectFdEventReactor*>(NULL),
        dynamic_cast<server::SelectFdEventReactor*>(reactor));
    delete reactor;
}

TEST(FdEventReactorFactory, CreateEpollDependsOnPlatform)
{
#ifdef __linux__
    server::FdEventReactor* reactor =
        server::FdEventReactorFactory::createEpoll();
    ASSERT_NE(static_cast<server::FdEventReactor*>(NULL), reactor);
    EXPECT_NE(static_cast<server::EpollFdEventReactor*>(NULL),
        dynamic_cast<server::EpollFdEventReactor*>(reactor));
    delete reactor;
#else
    EXPECT_ANY_THROW(server::FdEventReactorFactory::createEpoll());
#endif
}

TEST(FdEventReactorFactory, CreateKqueueDependsOnPlatform)
{
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
    server::FdEventReactor* reactor =
        server::FdEventReactorFactory::createKqueue();
    ASSERT_NE(static_cast<server::FdEventReactor*>(NULL), reactor);
    EXPECT_NE(static_cast<server::KqueueFdEventReactor*>(NULL),
        dynamic_cast<server::KqueueFdEventReactor*>(reactor));
    delete reactor;
#else
    EXPECT_ANY_THROW(server::FdEventReactorFactory::createKqueue());
#endif
}

}  // namespace
