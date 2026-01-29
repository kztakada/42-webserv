#include "server/reactor/fd_event_reactor/epoll_reactor.hpp"

#include <unistd.h>

#include <cassert>
#include <stdexcept>

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

    Result<uint32_t> add_mask_result = fdEventTypeToMask(fd_event.type);
    if (add_mask_result.isError())
        return Result<void>(ERROR, add_mask_result.getErrorMessage());
    uint32_t add_mask = add_mask_result.unwrap();

    if (event_registry_.find(fd) == event_registry_.end())
    {
        FdWatch* new_watch = new FdWatch(fd, add_mask, fd_event.session);
        event_registry_[fd] = new_watch;
        register_events = new_watch->events;
    }
    else
    {
        FdWatch* existing_watch = event_registry_[fd];
        uint32_t new_events = existing_watch->events | add_mask;
        existing_watch->events = new_events;
        operation = EPOLL_CTL_MOD;
        register_events = new_events;
    }

    struct epoll_event ev;
    ev.events = 0;
    ev.data.ptr = static_cast<void*>(event_registry_[fd]);

    if (register_events & kReadEventMask)
        ev.events |= EPOLLIN;
    if (register_events & kWriteEventMask)
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
    Result<uint32_t> remove_mask_result = fdEventTypeToMask(fd_event.type);
    if (remove_mask_result.isError())
        return Result<void>(ERROR, remove_mask_result.getErrorMessage());
    uint32_t remove_mask = remove_mask_result.unwrap();

    uint32_t new_events = existing_watch->events & (~remove_mask);
    struct epoll_event* ev = NULL;
    if ((new_events & (kReadEventMask | kWriteEventMask)) == 0)
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
        if (new_events & kReadEventMask)
            event.events |= EPOLLIN;
        if (new_events & kWriteEventMask)
            event.events |= EPOLLOUT;
        event.events |= EPOLLRDHUP;  // ハーフclose検出
        ev = &event;
    }
    if (epoll_ctl(epoll_fd_, operation, fd, ev) < 0)
        return Result<void>(ERROR, "removeWatch: epoll_ctl failed");

    return Result<void>();
}

Result<void> EpollFdEventReactor::deleteWatch(int fd)
{
    if (event_registry_.find(fd) == event_registry_.end())
        return Result<void>(ERROR, "deleteWatch: fd not found");

    FdWatch* existing_watch = event_registry_[fd];
    delete existing_watch;
    event_registry_.erase(fd);

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, NULL) < 0)
        return Result<void>(ERROR, "deleteWatch: epoll_ctl failed");

    return Result<void>();
}

Result<const std::vector<FdEvent>&> EpollFdEventReactor::waitEvents(
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

        // エラー優先: EPOLLERR が立っている場合は error イベント単独のみを返す
        if (events[i].events & EPOLLERR)
        {
            FdEvent error_event;
            error_event.fd = watch->fd;
            error_event.type = kErrorEvent;
            error_event.session = watch->session;
            error_event.is_opposite_close = is_opposite_close;
            occurred_events_.push_back(error_event);
            continue;
        }

        // OS(epoll)のイベントビットをプロジェクト内のFdEventTypeビットに変換
        uint32_t triggered = 0;
        if (events[i].events & EPOLLIN)
            triggered |= kReadEventMask;
        if (events[i].events & EPOLLOUT)
            triggered |= kWriteEventMask;

        std::vector<FdEvent> fd_events =
            watch->makeFdEvents(triggered, is_opposite_close);
        for (size_t j = 0; j < fd_events.size(); ++j)
            occurred_events_.push_back(fd_events[j]);
    }

    return occurred_events_;
}

}  // namespace server