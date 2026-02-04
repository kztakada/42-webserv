#ifndef WEBSERV_LISTENER_SESSION_HPP_
#define WEBSERV_LISTENER_SESSION_HPP_

#include "server/reactor/fd_event.hpp"
#include "server/request_router/request_router.hpp"
#include "server/session/fd/tcp_socket/tcp_listen_socket_fd.hpp"
#include "server/session/fd_session.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;

const int kNoTimeoutSeconds = 0;

// リスナーセッション：新規接続を受け付ける
class ListenerSession : public FdSession
{
   public:
    explicit ListenerSession(TcpListenSocketFd* listen_fd,
        FdSessionController& controller, const RequestRouter& router);
    virtual ~ListenerSession();

    virtual bool isTimedOut() const;  // リスナーセッションはタイムアウトしない

    virtual Result<void> handleEvent(const FdEvent& event);

    virtual bool isComplete() const;  // リスナーセッションは常にアクティブ

    virtual void getInitialWatchSpecs(std::vector<FdWatchSpec>* out) const;

   private:
    // --- ファイル記述子 (RAIIオブジェクト) ---
    TcpListenSocketFd* listen_fd_;  // 待ち受け用のソケット (bind/listen済み)

    // --- 制御と外部連携 ---
    const RequestRouter& router_;  // Serverから渡される参照、HttpSession生成用

    void acceptNewConnection();

    ListenerSession();
    ListenerSession(const ListenerSession& rhs);
    ListenerSession& operator=(const ListenerSession& rhs);
};

}  // namespace server

#endif
