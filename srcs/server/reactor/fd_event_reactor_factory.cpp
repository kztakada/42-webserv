#include "server/reactor/fd_event_reactor_factory.hpp"

#include <stdexcept>

#ifdef __linux__
#include "server/reactor/fd_event_reactor/epoll_reactor.hpp"
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
#include "server/reactor/fd_event_reactor/kqueue_reactor.hpp"
#endif

#include "server/reactor/fd_event_reactor/select_reactor.hpp"

namespace server
{

FdEventReactor* FdEventReactorFactory::create()
{
#ifdef __linux__
    return new EpollFdEventReactor();
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
    return new KqueueFdEventReactor();
#else
    return new SelectFdEventReactor();
#endif
}

FdEventReactor* FdEventReactorFactory::createEpoll()
{
#ifdef __linux__
    return new EpollFdEventReactor();
#else
    throw std::runtime_error("epoll is not available on this platform");
#endif
}

FdEventReactor* FdEventReactorFactory::createSelect()
{
    return new SelectFdEventReactor();
}

FdEventReactor* FdEventReactorFactory::createKqueue()
{
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
    return new KqueueFdEventReactor();
#else
    throw std::runtime_error("kqueue is not available on this platform");
#endif
}

}  // namespace server
