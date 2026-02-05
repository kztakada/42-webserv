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
#include "server/session/fd_session/http_session/http_handler.hpp"
#include "server/session/fd_session/http_session/http_response_writer.hpp"
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

    HttpSession(int fd, const SocketAddress& server_addr,
        const SocketAddress& client_addr, FdSessionController& controller,
        const RequestRouter& router);
    virtual ~HttpSession();

    virtual Result<void> handleEvent(const FdEvent& event);
    virtual bool isComplete() const;

    virtual void getInitialWatchSpecs(std::vector<FdWatchSpec>* out) const;

    // CgiSession からの通知: CGI stdout のヘッダが確定した
    Result<void> onCgiHeadersReady(CgiSession& cgi);

    // CgiSession からの通知: CGI がヘッダ確定前に失敗した
    Result<void> onCgiError(CgiSession& cgi, const std::string& message);

    HttpHandler& handler() { return handler_; }
    const HttpHandler& handler() const { return handler_; }

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

    bool socket_watch_read_;
    bool socket_watch_write_;

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
    bool peer_closed_;
    bool should_close_connection_;

    HttpSession();
    HttpSession(const HttpSession& rhs);
    HttpSession& operator=(const HttpSession& rhs);

    Result<void> startCgi_();
    Result<http::HttpRequest> buildInternalRedirectRequest_(
        const std::string& uri_path) const;

    // recv_buffer_ の残りだけで進められる分を処理する（read()しない）。
    // keep-alive + pipelining 等で「次リクエストが既に user-space
    // にある」場合の 停滞を防ぐ。
    Result<void> consumeRecvBufferWithoutRead_();

    // request_ が parse 完了した後の処理（routing/processor/CGI開始/response
    // writer 構築）。
    Result<void> prepareResponseOrCgi_();

    Result<void> updateSocketWatches_();

    // RequestProcessor::Output を使って body_source_ / response_writer_
    // を差し替える。 既存の writer/body_source は必要に応じて破棄する。
    void installBodySourceAndWriter_(BodySource* body_source);

    // processError を試み、失敗したら setSimpleErrorResponse_
    // で最低限のヘッダだけ作る。 fallback の場合 out->body_source は NULL
    // になる。
    Result<void> buildErrorOutput_(
        http::HttpStatus status, RequestProcessor::Output* out);

    // handler_.onRequestReady() を実行し、失敗時は 400
    // の最小レスポンスを用意する。 どちらの場合でも response_ の HTTP version
    // は request_ に合わせて設定する。
    Result<void> prepareRequestReadyOrBadRequest_();

    // RequestProcessor::process を実行し、失敗したら 500 を用意する。
    // この場合は接続を close する方針なので out->should_close_connection を
    // true にする。
    Result<void> buildProcessorOutputOrServerError_(
        RequestProcessor::Output* out);

    // upload_store + multipart/form-data の場合、一時ファイルに保存された
    // multipart envelope から file part だけを抽出し、destination に保存する。
    Result<void> finalizeUploadStoreIfNeeded_();

    // ---- internal helpers ----
    static http::HttpResponseEncoder::Options makeEncoderOptions_(
        const http::HttpRequest& request);
    static Result<void> setSimpleErrorResponse_(
        http::HttpResponse& response, http::HttpStatus status);

    // ---- event handlers (state-based) ----
    Result<void> handleExecuteCgiEvent_(const FdEvent& event);
    Result<void> handleRecvRequestReadEvent_(const FdEvent& event);
    Result<void> handleSendResponseWriteEvent_(const FdEvent& event);

    // ---- cgi notify handlers (split by responsibility) ----
    Result<void> handleCgiHeadersReadyLocalRedirect_(
        CgiSession& cgi, const http::CgiResponse& cr);
    Result<void> handleCgiHeadersReadyNormal_(
        CgiSession& cgi, const http::CgiResponse& cr);
    Result<void> handleCgiError_(CgiSession& cgi, const std::string& message);
};

}  // namespace server

#endif  // HTTP_SESSION_HPP_
