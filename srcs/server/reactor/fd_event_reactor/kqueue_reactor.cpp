#include <unistd.h>

#include "server/server_detail/fd_event_manager_detail/kqueue_fd_event_manager.hpp"
#include "utils/time.hpp"

namespace server
{
namespace server_detail
{
namespace detail
{

KqueueFdEventReactor::KqueueFdEventReactor() : kq_(kqueue())
{
    if (kq_ < 0)
    {
        throw std::runtime_error("kqueue() failed");
    }
}

KqueueFdEventReactor::~KqueueFdEventReactor() { close(kq_); }

Result<void> KqueueFdEventReactor::add(FdSession* session)
{
    sessions_[session->fd] = session;
    updateEvents(session);
    return Result<void>();
}

void KqueueFdEventReactor::remove(FdSession* session)
{
    // kqueueからイベントを削除
    struct kevent kev[2];
    int n = 0;

    EV_SET(&kev[n++], session->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&kev[n++], session->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    kevent(kq_, kev, n, NULL, 0, NULL);
    sessions_.erase(session->fd);
}

void KqueueFdEventReactor::setWatchedEvents(
    FdSession* session, unsigned int events)
{
    session->watched_events = events;
    updateEvents(session);
}

void KqueueFdEventReactor::addWatchedEvents(
    FdSession* session, unsigned int events)
{
    session->watched_events |= events;
    updateEvents(session);
}

void KqueueFdEventReactor::removeWatchedEvents(
    FdSession* session, unsigned int events)
{
    session->watched_events &= ~events;
    updateEvents(session);
}

void KqueueFdEventReactor::setTimeout(FdSession* session, long timeout_ms)
{
    session->timeout_ms = timeout_ms;
}

void KqueueFdEventReactor::updateEvents(FdSession* session)
{
    struct kevent kev[2];
    int n = 0;

    if (session->watched_events & kReadEvent)
    {
        EV_SET(&kev[n++], session->fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
            session);
    }
    else
    {
        EV_SET(&kev[n++], session->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    }

    if (session->watched_events & kWriteEvent)
    {
        EV_SET(&kev[n++], session->fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0,
            session);
    }
    else
    {
        EV_SET(&kev[n++], session->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    }

    kevent(kq_, kev, n, NULL, 0, NULL);
}

Result<std::vector<FdEvent>> KqueueFdEventReactor::waitFdEvents(int timeout_ms)
{
    const int kMaxEvents = 64;
    struct kevent events[kMaxEvents];

    struct timespec ts;
    struct timespec* ts_ptr = NULL;
    if (timeout_ms >= 0)
    {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        ts_ptr = &ts;
    }

    int nev = kevent(kq_, NULL, 0, events, kMaxEvents, ts_ptr);
    if (nev < 0)
    {
        return Result<std::vector<FdEvent>>(ERROR, "kevent wait failed");
    }

    std::vector<FdEvent> fd_events;
    for (int i = 0; i < nev; ++i)
    {
        FdSession* session = static_cast<FdSession*>(events[i].udata);
        if (session)
        {
            unsigned int triggered = 0;

            if (events[i].filter == EVFILT_READ)
            {
                triggered |= kReadEvent;
            }
            if (events[i].filter == EVFILT_WRITE)
            {
                triggered |= kWriteEvent;
            }
            if (events[i].flags & EV_ERROR)
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
    }

    return Result<std::vector<FdEvent>>(fd_events);
}

std::vector<FdEvent> KqueueFdEventReactor::getTimeoutEvents()
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

FdSession* KqueueFdEventReactor::findByFd(int fd) const
{
    std::map<int, FdSession*>::const_iterator it = sessions_.find(fd);
    return (it != sessions_.end()) ? it->second : NULL;
}

}  // namespace detail
}  // namespace server_detail
}  // namespace server