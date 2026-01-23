#ifndef WEBSERV_LISTENER_SESSION_HPP_
#define WEBSERV_LISTENER_SESSION_HPP_

#include "server/reactor/fd_event.hpp"
#include "server/request_router/request_router.hpp"
#include "server/session/fd/tcp_socket/tcp_listen_socket_fd.hpp"
#include "server/session/fd_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;

const int kInfiniteTimeout = 99999999;

// リスナーセッション：新規接続を受け付ける
class ListenerSession : public FdSession
{
   private:
    // --- ファイル記述子 (RAIIオブジェクト) ---
    TcpListenSocketFd listen_fd_;  // 待ち受け用のソケット (bind/listen済み)

    // --- 制御と外部連携 ---
    FdSessionController& controller_;  // 監督者への参照
    const RequestRouter& router_;  // Serverから渡される参照、HttpSession生成用

    virtual Result<void> onReadable();
    virtual Result<void> onWritable();
    virtual Result<void> onError();
    virtual Result<void> onTimeout();

    void acceptNewConnection();

   public:
    explicit ListenerSession(int fd, const SocketAddress& listen_addr,
        FdSessionController& controller, const RequestRouter& router)
        : FdSession(kInfiniteTimeout),
          listen_fd_(fd, listen_addr),
          controller_(controller),
          router_(router)
    {
        updateLastActiveTime();
    };
    virtual ~ListenerSession() {
    };  // listen_fd_ のデストラクタで自動的に close される

    virtual bool isTimedOut() const
    {
        return false;
    };  // リスナーセッションはタイムアウトしない

    virtual Result<void> handleEvent(FdEvent* event, uint32_t triggered_events);

    virtual bool isComplete() const
    {
        return false;
    };  // リスナーセッションは常にアクティブ

   private:
    ListenerSession();
    ListenerSession(const ListenerSession& rhs);
    ListenerSession& operator=(const ListenerSession& rhs);
};

}  // namespace server

#endif
