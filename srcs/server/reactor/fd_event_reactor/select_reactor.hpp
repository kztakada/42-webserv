#ifndef WEBSERV_SELECT_FD_EVENT_REACTOR_HPP_
#define WEBSERV_SELECT_FD_EVENT_REACTOR_HPP_

#include <sys/select.h>

#include <map>

#include "server/reactor/fd_event_reactor.hpp"

namespace server
{

class SelectFdEventReactor : public FdEventReactor
{
   public:
    SelectFdEventReactor();
    ~SelectFdEventReactor() override;

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
    std::map<int, FdSession*> sessions_;
    int max_fd_;

    void updateMaxFd();

    SelectFdEventReactor(const SelectFdEventReactor&);
    SelectFdEventReactor& operator=(const SelectFdEventReactor&);
};

}  // namespace server

#endif