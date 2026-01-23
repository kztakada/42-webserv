// server/server_detail/fd_event_manager_detail/select_fd_event_manager.cpp
#include <sys/time.h>

#include <algorithm>

#include "server/server_detail/fd_event_manager_detail/select_fd_event_manager.hpp"
#include "utils/time.hpp"

namespace server
{
namespace server_detail
{
namespace detail
{

SelectFdEventReactor::SelectFdEventReactor() : max_fd_(-1) {}

SelectFdEventReactor::~SelectFdEventReactor() {}

Result<void> SelectFdEventReactor::add(FdSession* session)
{
    sessions_[session->fd] = session;
    updateMaxFd();
    return Result<void>();
}

void SelectFdEventReactor::remove(FdSession* session)
{
    sessions_.erase(session->fd);
    updateMaxFd();
}

void SelectFdEventReactor::setWatchedEvents(
    FdSession* session, unsigned int events)
{
    session->watched_events = events;
}

void SelectFdEventReactor::addWatchedEvents(
    FdSession* session, unsigned int events)
{
    session->watched_events |= events;
}

void SelectFdEventReactor::removeWatchedEvents(
    FdSession* session, unsigned int events)
{
    session->watched_events &= ~events;
}

void SelectFdEventReactor::setTimeout(FdSession* session, long timeout_ms)
{
    session->timeout_ms = timeout_ms;
}

Result<std::vector<FdEvent>> SelectFdEventReactor::waitFdEvents(int timeout_ms)
{
    fd_set readfds, writefds, errorfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);

    // fd_setを構築
    for (std::map<int, FdSession*>::iterator it = sessions_.begin();
        it != sessions_.end(); ++it)
    {
        FdSession* session = it->second;
        if (session->watched_events & kReadEvent)
        {
            FD_SET(session->fd, &readfds);
        }
        if (session->watched_events & kWriteEvent)
        {
            FD_SET(session->fd, &writefds);
        }
        FD_SET(session->fd, &errorfds);
    }

    // タイムアウト設定
    struct timeval tv;
    struct timeval* tv_ptr = NULL;
    if (timeout_ms >= 0)
    {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }

    int ret = select(max_fd_ + 1, &readfds, &writefds, &errorfds, tv_ptr);
    if (ret < 0)
    {
        return Result<std::vector<FdEvent>>(ERROR, "select wait failed");
    }

    std::vector<FdEvent> fd_events;
    for (std::map<int, FdSession*>::iterator it = sessions_.begin();
        it != sessions_.end(); ++it)
    {
        FdSession* session = it->second;
        unsigned int triggered = 0;

        if (FD_ISSET(session->fd, &readfds))
        {
            triggered |= kReadEvent;
        }
        if (FD_ISSET(session->fd, &writefds))
        {
            triggered |= kWriteEvent;
        }
        if (FD_ISSET(session->fd, &errorfds))
        {
            triggered |= kErrorEvent;
        }

        if (triggered != 0)
        {
            FdEvent event;
            event.session = session;
            event.triggered_events = triggered;
            fd_events.push_back(event);
        }
    }

    return Result<std::vector<FdEvent>>(fd_events);
}

std::vector<FdEvent> SelectFdEventReactor::getTimeoutEvents()
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

FdSession* SelectFdEventReactor::findByFd(int fd) const
{
    std::map<int, FdSession*>::const_iterator it = sessions_.find(fd);
    return (it != sessions_.end()) ? it->second : NULL;
}

void SelectFdEventReactor::updateMaxFd()
{
    max_fd_ = -1;
    for (std::map<int, FdSession*>::iterator it = sessions_.begin();
        it != sessions_.end(); ++it)
    {
        if (it->first > max_fd_)
        {
            max_fd_ = it->first;
        }
    }
}

}  // namespace detail
}  // namespace server_detail
}  // namespace server