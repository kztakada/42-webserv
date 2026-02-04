#ifndef WEBSERV_FD_SESSION_HPP_
#define WEBSERV_FD_SESSION_HPP_

#include <ctime>
#include <vector>

#include "server/reactor/fd_event.hpp"
#include "utils/result.hpp"

// セッションとは、
// １つのファイルディスクリプタについての生存期間中の状態と動作を管理するオブジェクトです。
// 例えば、TCP接続セッション、CGIセッションなどがこれに該当します。

namespace server
{
using namespace utils::result;

class FdSessionController;

// セッションの基底抽象クラス
class FdSession
{
   public:
    struct FdWatchSpec
    {
        int fd;
        bool watch_read;
        bool watch_write;

        FdWatchSpec() : fd(-1), watch_read(false), watch_write(false) {}
        FdWatchSpec(int fd, bool watch_read, bool watch_write)
            : fd(fd), watch_read(watch_read), watch_write(watch_write)
        {
        }
    };

   protected:
    time_t last_active_time_;
    int timeout_seconds_;

    // --- 制御と外部連携 ---
    FdSessionController& controller_;  // 監督者への参照

   public:
    explicit FdSession(FdSessionController& controller, int timeout_seconds)
        : last_active_time_(time(NULL)),
          timeout_seconds_(timeout_seconds),
          controller_(controller) {};
    virtual ~FdSession() {};

    // タイムアウト管理
    void updateLastActiveTime() { last_active_time_ = time(NULL); }
    time_t getLastActiveTime() const { return last_active_time_; }
    int getTimeoutSeconds() const { return timeout_seconds_; }
    virtual bool isTimedOut() const
    {
        if (timeout_seconds_ <= 0)
            return false;
        time_t current_time = time(NULL);
        return (current_time - last_active_time_) > timeout_seconds_;
    }

    // イベント振り分け
    virtual Result<void> handleEvent(const FdEvent& event) = 0;

    // セッション状態
    virtual bool isComplete() const = 0;

    // Controller に委譲された直後に登録すべき watch を返す。
    // 返却する fd は「OSの fd」であり、close は各 Session の責務。
    virtual void getInitialWatchSpecs(std::vector<FdWatchSpec>* out) const
    {
        (void)out;
    }

   private:
    FdSession();
    FdSession(const FdSession& rhs);
    FdSession& operator=(const FdSession& rhs);
};

}  // namespace server

#endif  // FD_SESSION_HPP_
