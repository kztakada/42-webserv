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
    virtual ~SelectFdEventReactor();

    // 管理するイベントの追加・削除
    virtual Result<void> addWatch(FdEvent fd_event);
    virtual Result<void> removeWatch(FdEvent fd_event);
    virtual Result<void> deleteWatch(int fd);  // 途中解除(接続中止)用途

    // イベント待機
    virtual Result<const std::vector<FdEvent>&> waitEvents(int timeout_ms = 0);

   private:
    int max_fd_;

    void updateMaxFd();

    // コピー禁止
    SelectFdEventReactor(const SelectFdEventReactor&);
    SelectFdEventReactor& operator=(const SelectFdEventReactor&);
};

}  // namespace server

#endif