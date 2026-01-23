#ifndef WEBSERV_HTTP_SESSION_HPP_
#define WEBSERV_HTTP_SESSION_HPP_

#include <deque>

#include "http/http_request.hpp"
#include "http/http_response.hpp"
#include "server/reactor/fd_event.hpp"
#include "server/request_router/request_router.hpp"
#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"
#include "server/session/fd_session.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/io_buffer.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;
using namespace http;

// HTTPセッション：HTTPリクエスト/レスポンスの処理状態を管理
class HttpSession : public FdSession
{
   public:
    static const long kDefaultTimeoutSec = 5;

   private:
    // --- 所有しているリソース ---
    TcpConnectionSocketFd socket_fd_;  // クライアントとのソケット
    IoBuffer recv_buffer_;             // クライアントからのデータ受信用
    IoBuffer send_buffer_;             // クライアントへのデータ送信用

    // --- プロトコル解析・構築 ---
    HttpRequest request_;    // 解析済みのリクエスト
    HttpResponse response_;  // 構築中のレスポンス

    // --- 状態・制御 ---
    enum State
    {
        RECV_REQUEST,
        EXECUTE_CGI,
        SEND_RESPONSE,
        CLOSE_WAIT
    } state_;                          // 現在のセッション状態
    CgiSession* active_cgi_session_;   // 実行中のCGI（なければNULL）
    int redirect_count_;               // 内部リダイレクト回数
    FdSessionController& controller_;  // 監督者への参照
    const RequestRouter& router_;      // Serverから渡される参照

    // 簡易的な状態管理（実際のHTTPリクエスト/レスポンス処理は省略）
    bool has_request_;
    bool has_response_;
    bool connection_close_;
    bool waiting_for_cgi_;

    virtual Result<void> onReadable();
    virtual Result<void> onWritable();
    virtual Result<void> onError();
    virtual Result<void> onTimeout();

   public:
    HttpSession(int fd, const SocketAddress& server_addr,
        const SocketAddress& client_addr, FdSessionController& controller,
        const RequestRouter& router)
        : socket_fd_(fd, server_addr, client_addr),
          controller_(controller),
          router_(router),
          FdSession(kDefaultTimeoutSec)
    {
        updateLastActiveTime();
    };
    virtual ~HttpSession();

    virtual Result<void> handleEvent(FdEvent* event, uint32_t triggered_events);
    virtual bool isComplete() const;

    bool isWaitingForCgi() const;
    void setWaitingForCgi(bool waiting);

   private:
    HttpSession();
    HttpSession(const HttpSession& rhs);
    HttpSession& operator=(const HttpSession& rhs);
};

}  // namespace server

#endif  // HTTP_SESSION_HPP_
