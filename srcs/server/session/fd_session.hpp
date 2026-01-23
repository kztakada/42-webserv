#ifndef WEBSERV_FD_SESSION_HPP_
#define WEBSERV_FD_SESSION_HPP_

#include <ctime>

#include "server/reactor/fd_event.hpp"
#include "utils/result.hpp"

// セッションとは、
// １つのファイルディスクリプタについての生存期間中の状態と動作を管理するオブジェクトです。
// 例えば、TCP接続セッション、CGIセッションなどがこれに該当します。

namespace server
{
using namespace utils::result;

// セッションの基底抽象クラス
class FdSession
{
   protected:
    time_t last_active_time_;
    int timeout_seconds_;

    // イベントハンドラ（派生クラスで実装）
    virtual Result<void> onReadable() = 0;
    virtual Result<void> onWritable() = 0;
    virtual Result<void> onError() = 0;
    virtual Result<void> onTimeout() = 0;

   public:
    explicit FdSession(int timeout_seconds)
        : last_active_time_(time(NULL)), timeout_seconds_(timeout_seconds) {};
    virtual ~FdSession() {};

    // タイムアウト管理
    void updateLastActiveTime() { last_active_time_ = time(NULL); }
    virtual bool isTimedOut() const
    {
        time_t current_time = time(NULL);
        return (current_time - last_active_time_) > timeout_seconds_;
    }

    // イベント振り分け
    virtual Result<void> handleEvent(FdEvent* event,
        uint32_t triggered_events) = 0;  // FdEventのRoleを見て振り分けさせる

    // セッション状態
    virtual bool isComplete() const = 0;

   private:
    FdSession();
    FdSession(const FdSession& rhs);
    FdSession& operator=(const FdSession& rhs);
};

}  // namespace server

#endif  // FD_SESSION_HPP_
