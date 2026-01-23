#ifndef WEBSERV_KQUEUE_FD_EVENT_REACTOR_HPP_
#define WEBSERV_KQUEUE_FD_EVENT_REACTOR_HPP_

#include <sys/event.h>

#include <map>

#include "server/reactor/fd_event_reactor.hpp"

namespace server
{

class KqueueFdEventReactor : public FdEventReactor
{
   public:
    KqueueFdEventReactor();
    ~KqueueFdEventReactor() override;

    Result<void> add(FdSession* session) override;
    void remove(FdSession* session) override;
    void setWatchedEvents(FdSession* session, unsigned int events) override;
    void addWatchedEvents(FdSession* session, unsigned int events) override;
    void removeWatchedEvents(FdSession* session, unsigned int events) override;
    void setTimeout(FdSession* session, long timeout_ms) override;
    Result<std::vector<FdEvent>> waitFdEvents(int timeout_ms = 0) override;
    std::vector<FdEvent> getTimeoutEvents() override;
    FdSession* findByFd(int fd) const override;

   private:
    int kq_;  // kqueueファイルディスクリプタ
    std::map<int, FdSession*> sessions_;

    void updateEvents(FdSession* session);

    KqueueFdEventReactor(const KqueueFdEventReactor&);
    KqueueFdEventReactor& operator=(const KqueueFdEventReactor&);
};

}  // namespace server

#endif