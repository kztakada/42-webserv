#ifndef WEBSERV_FD_SESSION_CONTROLLER_HPP_
#define WEBSERV_FD_SESSION_CONTROLLER_HPP_

#include <set>

#include "server/reactor/fd_event_reactor.hpp"
#include "server/session/fd_session.hpp"

namespace server
{
class FdSessionController
{
   private:
    FdEventReactor* reactor_;
    std::set<FdSession*> active_sessions_;

    // reactorへの操作依頼
    void RequestRemoval();
    void AddWatchedEvents(unsigned int events);
    void RemoveWatchedEvents(unsigned int events);
    void SetWatchedEvents(unsigned int events);

   public:
    FdSessionController();
    ~FdSessionController();

    // セッションからの操作依頼
    void addSession(FdSession* session);
    void removeSession(FdSession* session);
    Result<void> registerFd(int fd, FdEvent::Role role, FdSession* session);
    void unregisterFd(int fd);

    void dispatchEvents(
        std::vector<std::pair<FdEvent*, uint32_t>> occurred_events);

    void checkTimeouts();

    void clearAllSessions();
};
}  // namespace server

#endif