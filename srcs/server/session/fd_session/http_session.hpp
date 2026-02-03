#ifndef WEBSERV_HTTP_SESSION_HPP_
#define WEBSERV_HTTP_SESSION_HPP_

#include <deque>

#include "http/http_request.hpp"
#include "http/http_response.hpp"
#include "http/http_response_encoder.hpp"
#include "server/reactor/fd_event.hpp"
#include "server/request_processor/request_processor.hpp"
#include "server/request_router/request_router.hpp"
#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"
#include "server/session/fd_session.hpp"
#include "server/session/fd_session/http_session/body_source.hpp"
#include "server/session/fd_session/http_session/http_handler.hpp"
#include "server/session/fd_session/http_session/http_response_writer.hpp"
#include "server/session/io_buffer.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;
using namespace http;

class CgiSession;

// HTTPセッション：HTTPリクエスト/レスポンスの処理状態を管理
class HttpSession : public FdSession
{
   public:
    static const long kDefaultTimeoutSec = 5;
    static const int kMaxInternalRedirects = 5;

    // テスト用
    const HttpRequest& request() const { return request_; }
    const HttpResponse& response() const { return response_; }

   private:
    // --- プロトコル解析・構築 ---
    HttpRequest request_;    // 解析済みのリクエスト
    HttpResponse response_;  // 構築中のレスポンス

    // --- 所有しているリソース ---
    TcpConnectionSocketFd socket_fd_;  // クライアントとのソケット

    // --- 制御と外部連携 ---
    const RequestRouter& router_;  // ListenerSession経由で渡される参照
    HttpHandler handler_;
    RequestProcessor processor_;

    BodySource* body_source_;
    HttpResponseWriter* response_writer_;

    IoBuffer recv_buffer_;  // クライアントからのデータ受信用
    IoBuffer send_buffer_;  // クライアントへのデータ送信用

    // --- 状態・制御 ---
    enum State
    {
        RECV_REQUEST,
        EXECUTE_CGI,
        SEND_RESPONSE,
        CLOSE_WAIT
    } state_;             // 現在のセッション状態
    int redirect_count_;  // 内部リダイレクト回数

    CgiSession* active_cgi_session_;  // 実行中のCGI（なければNULL）

    // 簡易的な状態管理（実際のHTTPリクエスト/レスポンス処理は省略）
    bool has_request_;
    bool has_response_;
    bool connection_close_;
    bool waiting_for_cgi_;

    bool is_complete_;

   public:
    HttpSession(int fd, const SocketAddress& server_addr,
        const SocketAddress& client_addr, FdSessionController& controller,
        const RequestRouter& router)
        : FdSession(controller, kDefaultTimeoutSec),
          request_(),
          response_(),
          socket_fd_(fd, server_addr, client_addr),
          router_(router),
          handler_(request_, response_, router, socket_fd_.getServerIp(),
              socket_fd_.getServerPort(), this),
          processor_(
              router_, socket_fd_.getServerIp(), socket_fd_.getServerPort()),
          body_source_(NULL),
          response_writer_(NULL),
          recv_buffer_(),
          send_buffer_(),
          state_(RECV_REQUEST),
          redirect_count_(0),
          active_cgi_session_(NULL),
          has_request_(false),
          has_response_(false),
          connection_close_(false),
          waiting_for_cgi_(false),
          is_complete_(false)
    {
        updateLastActiveTime();
    };
    virtual ~HttpSession();

    virtual Result<void> handleEvent(const FdEvent& event);
    virtual bool isComplete() const;

    // CgiSession からの通知: CGI stdout のヘッダが確定した
    Result<void> onCgiHeadersReady(CgiSession& cgi);

    bool isWaitingForCgi() const;
    void setWaitingForCgi(bool waiting);

    HttpHandler& handler() { return handler_; }
    const HttpHandler& handler() const { return handler_; }

   private:
    HttpSession();
    HttpSession(const HttpSession& rhs);
    HttpSession& operator=(const HttpSession& rhs);

    Result<void> startCgi_();
    Result<http::HttpRequest> buildInternalRedirectRequest_(
        const std::string& uri_path) const;
};

}  // namespace server

#endif  // HTTP_SESSION_HPP_
