#include "server/reactor/fd_event_reactor/select_reactor.hpp"

#include <sys/time.h>
#include <unistd.h>

namespace server
{

SelectFdEventReactor::SelectFdEventReactor() : max_fd_(-1) {}

SelectFdEventReactor::~SelectFdEventReactor() { clearAllEvents(); }

Result<void> SelectFdEventReactor::addWatch(FdEvent fd_event)
{
    int fd = fd_event.fd;
    Result<uint32_t> add_mask_result = fdEventTypeToMask(fd_event.type);
    if (add_mask_result.isError())
        return Result<void>(ERROR, add_mask_result.getErrorMessage());
    uint32_t add_mask = add_mask_result.unwrap();

    if (event_registry_.find(fd) == event_registry_.end())
    {
        FdWatch* new_watch = new FdWatch(fd, add_mask, fd_event.session);
        event_registry_[fd] = new_watch;
    }
    else
    {
        FdWatch* existing_watch = event_registry_[fd];
        existing_watch->events = existing_watch->events | add_mask;
    }

    if (fd > max_fd_)
        max_fd_ = fd;
    return Result<void>();
}

Result<void> SelectFdEventReactor::removeWatch(FdEvent fd_event)
{
    int fd = fd_event.fd;
    if (event_registry_.find(fd) == event_registry_.end())
        return Result<void>(ERROR, "removeWatch: fd not found");

    Result<uint32_t> remove_mask_result = fdEventTypeToMask(fd_event.type);
    if (remove_mask_result.isError())
        return Result<void>(ERROR, remove_mask_result.getErrorMessage());
    uint32_t remove_mask = remove_mask_result.unwrap();

    FdWatch* existing_watch = event_registry_[fd];
    existing_watch->events = existing_watch->events & (~remove_mask);
    if ((existing_watch->events & (kReadEventMask | kWriteEventMask)) == 0)
    {
        delete existing_watch;
        event_registry_.erase(fd);
        if (fd == max_fd_)
            updateMaxFd();
    }
    return Result<void>();
}

Result<void> SelectFdEventReactor::deleteWatch(int fd)
{
    if (event_registry_.find(fd) == event_registry_.end())
        return Result<void>(ERROR, "deleteWatch: fd not found");

    FdWatch* existing_watch = event_registry_[fd];
    delete existing_watch;
    event_registry_.erase(fd);
    if (fd == max_fd_)
        updateMaxFd();
    return Result<void>();
}

void SelectFdEventReactor::updateMaxFd()
{
    int new_max = -1;
    for (std::map<int, FdWatch*>::const_iterator it = event_registry_.begin();
        it != event_registry_.end(); ++it)
    {
        if (it->first > new_max)
            new_max = it->first;
    }
    max_fd_ = new_max;
}

Result<const std::vector<FdEvent>&> SelectFdEventReactor::waitEvents(
    int timeout_ms)
{
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    for (std::map<int, FdWatch*>::const_iterator it = event_registry_.begin();
        it != event_registry_.end(); ++it)
    {
        FdWatch* watch = it->second;
        if (watch->events & kReadEventMask)
            FD_SET(watch->fd, &readfds);
        if (watch->events & kWriteEventMask)
            FD_SET(watch->fd, &writefds);
        FD_SET(watch->fd, &exceptfds);
    }

    struct timeval tv;
    struct timeval* tv_ptr = NULL;
    if (timeout_ms >= 0)
    {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }

    int nfds = 0;
    if (max_fd_ >= 0)
        nfds = select(max_fd_ + 1, &readfds, &writefds, &exceptfds, tv_ptr);
    else
        nfds = select(0, &readfds, &writefds, &exceptfds, tv_ptr);

    if (nfds < 0)
        return Result<const std::vector<FdEvent>&>(ERROR, "select failed");

    occurred_events_.clear();
    for (std::map<int, FdWatch*>::const_iterator it = event_registry_.begin();
        it != event_registry_.end(); ++it)
    {
        FdWatch* watch = it->second;
        bool is_opposite_close = false;

        if (FD_ISSET(watch->fd, &exceptfds))
        {
            FdEvent error_event;
            error_event.fd = watch->fd;
            error_event.type = kErrorEvent;
            error_event.session = watch->session;
            error_event.is_opposite_close = is_opposite_close;
            occurred_events_.push_back(error_event);
            continue;
        }

        uint32_t triggered = 0;
        if (FD_ISSET(watch->fd, &readfds))
            triggered |= kReadEventMask;
        if (FD_ISSET(watch->fd, &writefds))
            triggered |= kWriteEventMask;

        if (triggered == 0)
            continue;

        std::vector<FdEvent> fd_events =
            watch->makeFdEvents(triggered, is_opposite_close);
        for (size_t i = 0; i < fd_events.size(); ++i)
            occurred_events_.push_back(fd_events[i]);
    }

    return occurred_events_;
}

}  // namespace server
