#include "server/reactor/fd_event_reactor_factory.hpp"

#ifdef __linux__
#include "server/server_detail/fd_event_manager_detail/epoll_fd_event_manager.hpp"
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
#include "server/server_detail/fd_event_manager_detail/kqueue_fd_event_manager.hpp"
#endif

#include "server/server_detail/fd_event_manager_detail/select_fd_event_manager.hpp"

namespace server
{
namespace server_detail
{

FdEventReactor* FdEventReactorFactory::create()
{
#ifdef __linux__
    return new detail::EpollFdEventReactor();
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
    return new detail::KqueueFdEventReactor();
#else
    return new detail::SelectFdEventReactor();
#endif
}

FdEventReactor* FdEventReactorFactory::createEpoll()
{
#ifdef __linux__
    return new detail::EpollFdEventReactor();
#else
    throw std::runtime_error("epoll is not available on this platform");
#endif
}

FdEventReactor* FdEventReactorFactory::createSelect()
{
    return new detail::SelectFdEventReactor();
}

FdEventReactor* FdEventReactorFactory::createKqueue()
{
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
    return new detail::KqueueFdEventReactor();
#else
    throw std::runtime_error("kqueue is not available on this platform");
#endif
}

}  // namespace server_detail
}  // namespace server