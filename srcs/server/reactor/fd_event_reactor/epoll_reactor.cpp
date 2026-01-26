#include "server/reactor/fd_event_reactor/epoll_reactor.hpp"

#include <unistd.h>

#include <cassert>

namespace server
{
using namespace utils::result;

EpollFdEventReactor::EpollFdEventReactor()
    : epoll_fd_(epoll_create1(EPOLL_CLOEXEC))
{
    if (epoll_fd_ < 0)
    {
        throw std::runtime_error("epoll_create1 failed");
    }
}

EpollFdEventReactor::~EpollFdEventReactor()
{
    clearAllEvents();
    close(epoll_fd_);
}

Result<void> EpollFdEventReactor::addWatch(FdEvent fd_event)
{
    int fd = fd_event.fd;
    int operation = EPOLL_CTL_ADD;
    uint32_t register_events = 0;
    if (fd_event.type == kErrorEvent || fd_event.type == kTimeoutEvent)
        return Result<void>(ERROR, "addWatch: invalid event type");

    if (event_registry_.find(fd) == event_registry_.end())
    {
        FdWatch* new_watch = new FdWatch(fd, fd_event.events, fd_event.session);
        event_registry_[fd] = new_watch;
        register_events = new_watch->events;
    }
    else
    {
        FdWatch* existing_watch = event_registry_[fd];
        uint32_t new_events = existing_watch->events | fd_event.events;
        existing_watch->events = new_events;
        operation = EPOLL_CTL_MOD;
        register_events = new_events;
    }

    struct epoll_event ev;
    ev.events = 0;
    ev.data.ptr = static_cast<void*>(event_registry_[fd]);

    if (register_events & kReadEvent)
        ev.events |= EPOLLIN;
    if (register_events & kWriteEvent)
        ev.events |= EPOLLOUT;
    ev.events |= EPOLLRDHUP;  // ハーフclose検出

    if (epoll_ctl(epoll_fd_, operation, fd, &ev) < 0)
        return Result<void>(ERROR, "addWatch: epoll_ctl failed");

    return Result<void>();
}

Result<void> EpollFdEventReactor::removeWatch(FdEvent fd_event)
{
    int fd = fd_event.fd;
    int operation = EPOLL_CTL_MOD;
    if (event_registry_.find(fd) == event_registry_.end())
        return Result<void>(ERROR, "removeWatch: fd not found");

    FdWatch* existing_watch = event_registry_[fd];
    uint32_t new_events = existing_watch->events & (~fd_event.events);
    void* ev = NULL;
    if (new_events == 0 || new_events == kErrorEvent ||
        new_events == kTimeoutEvent ||
        new_events == (kErrorEvent | kTimeoutEvent))
    {
        delete existing_watch;
        event_registry_.erase(fd);
        operation = EPOLL_CTL_DEL;
    }
    else
    {
        existing_watch->events = new_events;
        struct epoll_event event;
        event.events = 0;
        event.data.ptr = static_cast<void*>(existing_watch);
        if (new_events & kReadEvent)
            event.events |= EPOLLIN;
        if (new_events & kWriteEvent)
            event.events |= EPOLLOUT;
        event.events |= EPOLLRDHUP;  // ハーフclose検出
        ev = &event;
    }
    if (epoll_ctl(epoll_fd_, operation, fd, ev) < 0)
        return Result<void>(ERROR, "removeWatch: epoll_ctl failed");

    return Result<void>();
}

Result<const std::vector<FdEvent>&> EpollFdEventReactor::waitFdEvents(
    int timeout_ms)
{
    struct epoll_event events[kMaxFetchFdEvents];

    int nfds = epoll_wait(epoll_fd_, events, kMaxFetchFdEvents, timeout_ms);
    if (nfds < 0)
    {
        return Result<const std::vector<FdEvent>&>(ERROR, "epoll_wait failed");
    }

    occurred_events_.clear();
    for (int i = 0; i < nfds; ++i)
    {
        FdWatch* watch = static_cast<FdWatch*>(events[i].data.ptr);
        bool is_opposite_close = (events[i].events & EPOLLRDHUP) != 0;
        std::vector<FdEvent> fd_events =
            watch->makeFdEvents(events[i].events, is_opposite_close);
        for (size_t j = 0; j < fd_events.size(); ++j)
        {
            occurred_events_.push_back(fd_events[j]);
        }
    }

    return occurred_events_;
}

}  // namespace server