#include "server/reactor/fd_watch.hpp"

namespace server
{
std::vector<FdEvent> FdWatch::makeFdEvents(
    uint32_t triggered_events, bool is_opposite_close)
{
    std::vector<FdEvent> fd_events;
    if (triggered_events & kReadEventMask)
    {
        FdEvent event;
        event.fd = fd;
        event.type = kReadEvent;
        event.session = session;
        event.is_opposite_close = is_opposite_close;
        fd_events.push_back(event);
    }
    if (triggered_events & kWriteEventMask)
    {
        FdEvent event;
        event.fd = fd;
        event.type = kWriteEvent;
        event.session = session;
        event.is_opposite_close = is_opposite_close;
        fd_events.push_back(event);
    }
    return fd_events;
}

}  // namespace server
