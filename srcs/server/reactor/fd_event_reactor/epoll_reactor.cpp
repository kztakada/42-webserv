#include <unistd.h>

#include <cassert>

#include "server/server_detail/fd_event_manager_detail/epoll_fd_event_manager.hpp"
#include "utils/time.hpp"

namespace server
{
namespace server_detail
{
namespace detail
{

EpollFdEventReactor::EpollFdEventReactor() : epfd_(epoll_create1(EPOLL_CLOEXEC))
{
    if (epfd_ < 0)
    {
        throw std::runtime_error("epoll_create1 failed");
    }
}

EpollFdEventReactor::~EpollFdEventReactor() { close(epfd_); }

Result<void> EpollFdEventReactor::add(FdSession* session)
{
    assert(sessions_.find(session->fd) == sessions_.end());

    struct epoll_event ev = toEpollEvent(session);

    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, session->fd, &ev) < 0)
        return Result<void>(ERROR, "epoll_ctl ADD failed");

    sessions_[session->fd] = session;
    return Result<void>();
}

void EpollFdEventReactor::remove(FdSession* session)
{
    sessions_.erase(session->fd);
    epoll_ctl(epfd_, EPOLL_CTL_DEL, session->fd, NULL);
}

void EpollFdEventReactor::setWatchedEvents(
    FdSession* session, unsigned int events)
{
    session->watched_events = events;

    struct epoll_event ev = toEpollEvent(session);
    epoll_ctl(epfd_, EPOLL_CTL_MOD, session->fd, &ev);
}

void EpollFdEventReactor::addWatchedEvents(
    FdSession* session, unsigned int events)
{
    setWatchedEvents(session, session->watched_events | events);
}

void EpollFdEventReactor::removeWatchedEvents(
    FdSession* session, unsigned int events)
{
    setWatchedEvents(session, session->watched_events & ~events);
}

void EpollFdEventReactor::setTimeout(FdSession* session, long timeout_ms)
{
    session->timeout_ms = timeout_ms;
}

Result<std::vector<FdEvent>> EpollFdEventReactor::waitFdEvents(int timeout_ms)
{
    const int kMaxEvents = 64;
    struct epoll_event events[kMaxEvents];

    int nfds = epoll_wait(epfd_, events, kMaxEvents, timeout_ms);
    if (nfds < 0)
    {
        return Result<std::vector<FdEvent>>(ERROR, "epoll_wait failed");
    }

    std::vector<FdEvent> fd_events;
    for (int i = 0; i < nfds; ++i)
    {
        int fd = events[i].data.fd;
        FdSession* session = findByFd(fd);
        if (session)
        {
            FdEvent event = fromEpollEvent(session, events[i]);
            if (event.triggered_events != 0)
            {
                fd_events.push_back(event);
            }
        }
    }

    return Result<std::vector<FdEvent>>(fd_events);
}

std::vector<FdEvent> EpollFdEventReactor::getTimeoutEvents()
{
    long current_time = utils::getCurrentTimeMs();
    std::vector<FdEvent> timeout_events;

    for (std::map<int, FdSession*>::iterator it = sessions_.begin();
        it != sessions_.end(); ++it)
    {
        FdSession* session = it->second;
        if (session->isTimedOut(current_time))
        {
            FdEvent event;
            event.session = session;
            event.triggered_events = kTimeoutEvent;
            timeout_events.push_back(event);
        }
    }

    return timeout_events;
}

FdSession* EpollFdEventReactor::findByFd(int fd) const
{
    std::map<int, FdSession*>::const_iterator it = sessions_.find(fd);
    return (it != sessions_.end()) ? it->second : NULL;
}

struct epoll_event EpollFdEventReactor::toEpollEvent(FdSession* session) const
{
    struct epoll_event ev;
    ev.events = 0;
    ev.data.fd = session->fd;

    if (session->watched_events & kReadEvent)
    {
        ev.events |= EPOLLIN;
    }
    if (session->watched_events & kWriteEvent)
    {
        ev.events |= EPOLLOUT;
    }
    ev.events |= EPOLLRDHUP;

    return ev;
}

FdEvent EpollFdEventReactor::fromEpollEvent(
    FdSession* session, const struct epoll_event& ev) const
{
    FdEvent event;
    event.session = session;
    event.triggered_events = 0;

    if ((ev.events & EPOLLIN) && (session->watched_events & kReadEvent))
    {
        event.triggered_events |= kReadEvent;
    }
    if ((ev.events & EPOLLOUT) && (session->watched_events & kWriteEvent))
    {
        event.triggered_events |= kWriteEvent;
    }
    if (ev.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
    {
        event.triggered_events |= kReadEvent | kErrorEvent;
    }

    return event;
}

}  // namespace detail
}  // namespace server_detail
}  // namespace server