#ifndef WEBSERV_FD_EVENT_REACTOR_FACTORY_HPP_
#define WEBSERV_FD_EVENT_REACTOR_FACTORY_HPP_

#include <memory>

#include "server/reactor/fd_event_reactor.hpp"

namespace server
{

class FdEventReactorFactory
{
   public:
    // プラットフォームに応じて最適な実装を自動選択
    static FdEventReactor* create();

    // 明示的に実装を指定
    static FdEventReactor* createEpoll();
    static FdEventReactor* createSelect();
    static FdEventReactor* createKqueue();

   private:
    FdEventReactorFactory();  // インスタンス化禁止
};

}  // namespace server

#endif
