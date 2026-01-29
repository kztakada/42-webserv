#ifndef WEBSERV_EPOLL_FD_EVENT_REACTOR_HPP_
#define WEBSERV_EPOLL_FD_EVENT_REACTOR_HPP_

#include <sys/epoll.h>

#include <map>

#include "server/reactor/fd_event_reactor.hpp"

namespace server
{
using namespace utils::result;

class EpollFdEventReactor : public FdEventReactor
{
   public:
    EpollFdEventReactor();
    virtual ~EpollFdEventReactor();

    // 管理するイベントの追加・削除
    virtual Result<void> addWatch(FdEvent fd_event);
    virtual Result<void> removeWatch(FdEvent fd_event);
    virtual Result<void> deleteWatch(int fd);  // 途中解除(接続中止)用途

    // イベント待機
    virtual Result<const std::vector<FdEvent>&> waitEvents(int timeout_ms = 0);

   private:
    int epoll_fd_;

    // コピー禁止
    EpollFdEventReactor(const EpollFdEventReactor&);
    EpollFdEventReactor& operator=(const EpollFdEventReactor&);
};

}  // namespace server

#endif