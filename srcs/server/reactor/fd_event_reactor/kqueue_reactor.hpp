#ifndef WEBSERV_KQUEUE_FD_EVENT_REACTOR_HPP_
#define WEBSERV_KQUEUE_FD_EVENT_REACTOR_HPP_

#include <sys/event.h>

#include <map>

#include "server/reactor/fd_event_reactor.hpp"

namespace server
{
using namespace utils::result;

class KqueueFdEventReactor : public FdEventReactor
{
   public:
    KqueueFdEventReactor();
    virtual ~KqueueFdEventReactor();

    // 管理するイベントの追加・削除
    virtual Result<void> addWatch(FdEvent fd_event);
    virtual Result<void> removeWatch(FdEvent fd_event);

    // イベント待機
    virtual Result<const std::vector<FdEvent>&> waitEvents(int timeout_ms = 0);

   private:
    int kq_;  // kqueueファイルディスクリプタ

    // コピー禁止
    KqueueFdEventReactor(const KqueueFdEventReactor&);
    KqueueFdEventReactor& operator=(const KqueueFdEventReactor&);
};

}  // namespace server

#endif