#ifndef WEBSERV_HTTP_SESSION_HPP_
#define WEBSERV_HTTP_SESSION_HPP_

#include <cstddef>
#include <deque>

#include "http/http_request.hpp"
#include "http/http_response.hpp"
#include "http/http_response_encoder.hpp"
#include "server/http_processing_module/http_processing_module.hpp"
#include "server/http_processing_module/request_dispatcher.hpp"
#include "server/http_processing_module/request_processor.hpp"
#include "server/http_processing_module/request_router/request_router.hpp"
#include "server/reactor/fd_event.hpp"
#include "server/session/fd/tcp_socket/tcp_connection_socket_fd.hpp"
#include "server/session/fd_session.hpp"
#include "server/session/fd_session/http_session/body_source.hpp"
#include "server/session/fd_session/http_session/http_response_writer.hpp"
#include "server/session/fd_session/http_session/session_context.hpp"
#include "server/session/fd_session/http_session/states/i_http_session_state.hpp"
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
    const HttpRequest& request() const { return context_.request; }
    const HttpResponse& response() const { return context_.response; }

    // コンストラクタ・デストラクタ
    HttpSession(int fd, const SocketAddress& server_addr,
        const SocketAddress& client_addr, FdSessionController& controller,
        HttpProcessingModule& module);
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

    SessionContext& getContext() { return context_; }
    const SessionContext& getContext() const { return context_; }

    friend class RecvRequestState;
    friend class ExecuteCgiState;
    friend class SendResponseState;
    friend class CloseWaitState;
    friend class SessionCgiHandler;
    friend class ProcessRequestAction;
    friend class SendErrorAction;
    friend class ExecuteCgiAction;

   private:
    SessionContext context_;

    HttpProcessingModule& module_;

    HttpProcessingModule& module() { return module_; }
    const HttpProcessingModule& module() const { return module_; }

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
};

}  // namespace server

#endif  // HTTP_SESSION_HPP_