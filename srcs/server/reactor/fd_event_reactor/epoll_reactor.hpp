#ifndef WEBSERV_EPOLL_FD_EVENT_REACTOR_HPP_
#define WEBSERV_EPOLL_FD_EVENT_REACTOR_HPP_

#include <sys/epoll.h>

#include <map>

#include "server/reactor/fd_event_reactor.hpp"

namespace server
{

class EpollFdEventReactor : public FdEventReactor
{
   public:
    EpollFdEventReactor();
    ~EpollFdEventReactor() override;

    Result<void> add(FdEvent* fd_event) override;
    void remove(FdEvent* fd_event) override;
    void setTimeout(FdSession* session, long timeout_ms) override;
    Result<std::vector<FdEvent>> waitFdEvents(int timeout_ms = 0) override;
    std::vector<FdEvent> getTimeoutEvents() override;
    FdSession* findByFd(int fd) const override;

   private:
    int epfd_;                            // epollファイルディスクリプタ
    std::map<int, FdSession*> sessions_;  // fd -> session

    // epoll_eventへの変換
    struct epoll_event toEpollEvent(FdSession* session) const;
    FdEvent fromEpollEvent(
        FdSession* session, const struct epoll_event& ev) const;

    // コピー禁止
    EpollFdEventReactor(const EpollFdEventReactor&);
    EpollFdEventReactor& operator=(const EpollFdEventReactor&);
};

}  // namespace server

#endif