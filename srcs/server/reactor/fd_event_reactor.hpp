#ifndef WEBSERV_FD_EVENT_REACTOR_HPP_
#define WEBSERV_FD_EVENT_REACTOR_HPP_

#include <map>
#include <vector>

#include "server/reactor/fd_event.hpp"
#include "utils/result.hpp"

namespace server
{

using namespace utils::result;

// イベントマネージャーの抽象インターフェース
class FdEventReactor
{
   protected:
    std::map<int, FdEvent*> event_registry_;
    std::vector<std::pair<FdEvent*, uint32_t>> occurred_events_;

   public:
    virtual ~FdEventReactor() {}

    // イベント待機
    virtual Result<std::vector<std::pair<FdEvent*, uint32_t>>> waitEvents(
        int timeout_ms = 0) = 0;

    // 管理するイベントの追加・削除
    virtual Result<void> updateEvent(FdEvent fd_event, uint32_t events) = 0;
    virtual void removeEvent(int fd) = 0;
    void clearAllEvents()
    {
        for (std::map<int, FdEvent*>::iterator it = event_registry_.begin();
            it != event_registry_.end(); ++it)
        {
            delete it->second;
        }
        event_registry_.clear();
    }
};

}  // namespace server

#endif