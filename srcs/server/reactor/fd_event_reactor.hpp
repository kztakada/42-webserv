#ifndef WEBSERV_FD_EVENT_REACTOR_HPP_
#define WEBSERV_FD_EVENT_REACTOR_HPP_

#include <map>
#include <vector>

#include "server/reactor/fd_event.hpp"
#include "utils/result.hpp"

namespace server
{

using namespace utils::result;

const int kMaxFetchFdEvents = 64;

// イベントマネージャーの抽象インターフェース
class FdEventReactor
{
   protected:
    std::map<int, FdWatch*> event_registry_;  // fd と監視中イベントの対応表
    std::vector<FdEvent> occurred_events_;    // 発生したイベントのリスト

   public:
    virtual ~FdEventReactor() {}

    // イベント待機
    virtual Result<const std::vector<FdEvent>&> waitEvents(
        int timeout_ms = 0) = 0;

    // 管理するイベントの追加・削除
    virtual Result<void> addWatch(FdEvent fd_event) = 0;
    virtual Result<void> removeWatch(FdEvent fd_event) = 0;
    void clearAllEvents()
    {
        for (std::map<int, FdWatch*>::iterator it = event_registry_.begin();
            it != event_registry_.end(); ++it)
        {
            delete it->second;
        }
        event_registry_.clear();
        occurred_events_.clear();
    }
};

}  // namespace server

#endif