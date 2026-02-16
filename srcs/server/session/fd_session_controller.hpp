#ifndef WEBSERV_FD_SESSION_CONTROLLER_HPP_
#define WEBSERV_FD_SESSION_CONTROLLER_HPP_

#include <map>
#include <set>
#include <vector>

#include "server/reactor/fd_event_reactor.hpp"
#include "server/session/fd_session.hpp"

namespace server
{
class FdSessionController
{
   public:
    FdSessionController();
    explicit FdSessionController(FdEventReactor* reactor, bool owns_reactor);
    ~FdSessionController();

    // 次にタイムアウトが発生し得るまでの待ち時間(ms)を返す。
    // - 0: 直ちにtimeoutチェックを走らせたい
    // - -1: timeoutが存在しない（無期限待ち）
    int getNextTimeoutMs() const;

    // --- Session ライフサイクル ---
    // 所有権を引き取り、必要な fd watch を登録する。
    Result<void> delegateSession(FdSession* session);

    // Session が自分自身の削除を依頼する。
    // dispatchEvents() 実行中は即時 delete せず、バッチ末尾で delete する。
    void requestDelete(FdSession* session);

    // --- fd watch 管理（Session からの依頼API）---
    // session と fd を紐付け、watch を設定する（初回/更新両対応）。
    Result<void> setWatch(
        int fd, FdSession* session, bool watch_read, bool watch_write);

    // 既存 fd の watch を更新する（session は紐付け済み前提）。
    Result<void> updateWatch(int fd, bool watch_read, bool watch_write);

    // fd を別 Session に付け替える（例: CGI stdout を HttpSession に渡す）。
    Result<void> rebindWatch(
        int fd, FdSession* new_session, bool watch_read, bool watch_write);

    // fd の watch を完全解除する（reactor から deleteWatch する）。
    void unregisterFd(int fd);

    // server(mainLoop)からの呼び出し
    void dispatchEvents(const std::vector<FdEvent>& occurred_events);
    void handleTimeouts();

    void clearAllSessions();

    bool isShuttingDown() const { return is_shutting_down_; }

   private:
    struct FdWatchState
    {
        FdSession* session;
        bool watch_read;
        bool watch_write;

        FdWatchState() : session(NULL), watch_read(false), watch_write(false) {}
        FdWatchState(FdSession* session, bool r, bool w)
            : session(session), watch_read(r), watch_write(w)
        {
        }
    };

    FdEventReactor* reactor_;
    bool owns_reactor_;
    std::set<FdSession*> active_sessions_;     // 所有している生存 Session
    std::set<FdSession*> deleting_sessions_;   // delete予定（dispatch中）
    std::vector<FdSession*> deferred_delete_;  // バッチ末尾delete

    std::map<int, FdWatchState> fd_watch_state_;        // fd -> watch状態
    std::map<FdSession*, std::set<int> > session_fds_;  // session -> fds

    bool is_shutting_down_;

    Result<void> addOrRemoveWatch_(
        int fd, FdSession* session, bool want_read, bool want_write);
    void detachFdFromSession_(int fd);
    void detachAllFdsFromSession_(FdSession* session);
    void detachAllFdsFromSessionFallback_(FdSession* session);
};
}  // namespace server

#endif
