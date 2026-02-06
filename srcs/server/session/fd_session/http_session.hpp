#ifndef WEBSERV_HTTP_SESSION_HPP_
#define WEBSERV_HTTP_SESSION_HPP_

#include <cstddef>
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
#include "server/session/fd_session/http_session/http_response_writer.hpp"
#include "server/session/fd_session/http_session/request_dispatcher.hpp"
#include "server/session/fd_session/http_session/states/i_http_session_state.hpp"
#include "server/session/fd_session/http_session/session_cgi_handler.hpp"
#include "server/session/io_buffer.hpp"
#include "utils/result.hpp"

namespace http
{
class CgiResponse;
}

namespace server
{
using namespace utils::result;
using namespace http;

class CgiSession;
class RecvRequestState;
class ExecuteCgiState;
class SendResponseState;
class CloseWaitState;
class ProcessRequestAction;
class SendErrorAction;
class ExecuteCgiAction;

// HTTPセッション：HTTPリクエスト/レスポンスの処理状態を管理
class HttpSession : public FdSession
{
   public:
    static const long kDefaultTimeoutSec = 5;
    static const int kMaxInternalRedirects = 5;
    static const size_t kMaxRecvBufferBytes = 64 * 1024;

    // テスト用
    const HttpRequest& request() const { return request_; }
    const HttpResponse& response() const { return response_; }

    // コンストラクタ・デストラクタ
    HttpSession(int fd, const SocketAddress& server_addr,
        const SocketAddress& client_addr, FdSessionController& controller,
        const RequestRouter& router);
    virtual ~HttpSession();

    // for Watch初期設定
    virtual void getInitialWatchSpecs(std::vector<FdWatchSpec>* out) const;

    // イベントハンドラ
    virtual Result<void> handleEvent(const FdEvent& event);

    // セッション完了判定
    virtual bool isComplete() const;

    // CGI通知ハンドラ
    Result<void> onCgiHeadersReady(CgiSession& cgi);
    Result<void> onCgiError(CgiSession& cgi, const std::string& message);

    // 状態遷移
    void changeState(IHttpSessionState* next_state);

    friend class RecvRequestState;
    friend class ExecuteCgiState;
    friend class SendResponseState;
    friend class CloseWaitState;
    friend class SessionCgiHandler;
    friend class ProcessRequestAction;
    friend class SendErrorAction;
    friend class ExecuteCgiAction;

   private:
    // --- プロトコル解析・構築 ---
    HttpRequest request_;    // 動的なHTTPリクエスト
    HttpResponse response_;  // 動的なHTTPレスポンス

    // --- 所有しているリソース ---
    TcpConnectionSocketFd socket_fd_;  // クライアントとのソケット

    // --- 制御と外部連携 ---
    const RequestRouter& router_;  // ListenerSession経由で渡される参照
    RequestProcessor processor_;   // リクエスト処理ユーティリティ

    // --- サブモジュール ---
    SessionCgiHandler cgi_handler_; // CGIハンドラ
    RequestDispatcher dispatcher_;  // リクエストディスパッチャ

    // --- データ転送関連 ---
    BodySource* body_source_;              // レスポンスボディ供給元（所有権あり）
    HttpResponseWriter* response_writer_;  // レスポンスエンコード・送信補助

    // ソケットI/O用バッファ
    IoBuffer recv_buffer_;  // クライアントからのデータ受信用
    IoBuffer send_buffer_;  // クライアントへのデータ送信用

    // --- 状態・制御 ---
    IHttpSessionState* current_state_;  // 現在のステート
    IHttpSessionState* pending_state_;  // 遷移予定のステート
    bool is_complete_;                  // 完了フラグ

    int redirect_count_;            // 内部リダイレクト回数
    bool peer_closed_;              // クライアントがcloseしたか
    bool should_close_connection_;  // レスポンス後に接続を閉じるべきか
    bool socket_watch_read_;        // ソケット read イベント監視フラグ
    bool socket_watch_write_;       // ソケット write イベント監視フラグ

    HttpSession();
    HttpSession(const HttpSession& rhs);
    HttpSession& operator=(const HttpSession& rhs);

    // http_session_redirect.cpp
    Result<http::HttpRequest> buildInternalRedirectRequest_(
        const std::string& uri_path) const;

    // http_session_watch.cpp
    Result<void> updateSocketWatches_();

    // http_session_prepare.cpp
    Result<void> consumeRecvBufferWithoutRead_();
    Result<void> prepareResponseOrCgi_();

    // http_session_helpers.cpp
    void installBodySourceAndWriter_(BodySource* body_source);
    Result<void> buildErrorOutput_(
        http::HttpStatus status, RequestProcessor::Output* out);
    Result<void> buildProcessorOutputOrServerError_(
        RequestProcessor::Output* out);
    static http::HttpResponseEncoder::Options makeEncoderOptions_(
        const http::HttpRequest& request);
    static Result<void> setSimpleErrorResponse_(
        http::HttpResponse& response, http::HttpStatus status);
};

}  // namespace server

#endif  // HTTP_SESSION_HPP_
