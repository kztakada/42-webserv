#ifndef WEBSERV_CGI_SESSION_HPP_
#define WEBSERV_CGI_SESSION_HPP_

#include <sys/types.h>

#include <vector>

#include "http/cgi_response.hpp"
#include "server/reactor/fd_event.hpp"
#include "server/session/fd/cgi_pipe/cgi_pipe_fd.hpp"
#include "server/session/fd_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "server/session/io_buffer.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;
using namespace http;

class HttpSession;

// CGIセッション：CGIプロセスとの通信状態を管理
class CgiSession : public FdSession
{
   public:
    static const long kDefaultTimeoutSec = 5;

   private:
    // --- 子プロセス管理 ---
    pid_t pid_;  // CGIプロセスのID (waitpid/kill用)

    // --- ファイル記述子 (RAIIオブジェクト) ---
    CgiPipeFd pipe_in_;   // CGIへの標準入力 (write)
    CgiPipeFd pipe_out_;  // CGIからの標準出力 (read)
    CgiPipeFd pipe_err_;  // CGIからの標準エラー (read)

    // --- バッファ群 ---
    IoBuffer stdin_buffer_;   // HTTP Request Body を溜めてCGIへ送る用
    IoBuffer stdout_buffer_;  // CGIからのレスポンスを溜めてパースする用
    IoBuffer stderr_buffer_;  // CGIからのエラーログを溜める用

    // --- プロトコル解析 ---
    CgiResponse cgi_response_;  // CGIヘッダーのパース、Local Redirect判定

    // --- 制御と外部連携 ---
    HttpSession* parent_session_;  // 親となるHttpSessionへのポインタ

    // HTTP Request Body を送る元（BodyStore の openForRead() のFD）。
    // -1 の場合はボディなし。
    int request_body_fd_;

    // --- 状態管理 ---
    bool is_stdout_eof_;
    bool is_stderr_eof_;
    bool input_complete_;
    bool headers_complete_;

    // ヘッダ終端と同じ read() で先読みしてしまった body の断片
    std::vector<utils::Byte> prefetched_body_;

   public:
    CgiSession(pid_t pid, int in_fd, int out_fd, int err_fd,
        int request_body_fd, HttpSession* parent,
        FdSessionController& controller)
        : FdSession(controller, kDefaultTimeoutSec),
          pid_(pid),
          pipe_in_(in_fd),
          pipe_out_(out_fd),
          pipe_err_(err_fd),
          stdin_buffer_(),
          stdout_buffer_(),
          stderr_buffer_(),
          cgi_response_(),
          parent_session_(parent),
          request_body_fd_(request_body_fd),
          is_stdout_eof_(false),
          is_stderr_eof_(false),
          input_complete_(false),
          headers_complete_(false),
          prefetched_body_()
    {
        updateLastActiveTime();
    }
    virtual ~CgiSession();  // ここで確実に kill(pid_) と close を行う

    virtual Result<void> handleEvent(const FdEvent& event);
    virtual bool isComplete() const;

    virtual void getInitialWatchSpecs(std::vector<FdWatchSpec>* out) const;

    HttpSession* getParentSession() const { return parent_session_; }

    bool isHeadersComplete() const { return headers_complete_; }
    const http::CgiResponse& response() const { return cgi_response_; }

    // stdout を HttpSession 側に移譲し、BodySource
    // でストリーミングするために使う。 以後この CgiSession は stdout
    // を読まない。
    int releaseStdoutFd() { return pipe_out_.release(); }

    // ヘッダ終端と同じ read で先読みした body を取り出す（1回限り）。
    std::vector<utils::Byte> takePrefetchedBody()
    {
        std::vector<utils::Byte> out;
        out.swap(prefetched_body_);
        return out;
    }

    int stdinFd() const { return pipe_in_.getFd(); }
    int stdoutFd() const { return pipe_out_.getFd(); }
    int stderrFd() const { return pipe_err_.getFd(); }

    pid_t pid() const { return pid_; }

   private:
    CgiSession();
    CgiSession(const CgiSession& rhs);
    CgiSession& operator=(const CgiSession& rhs);

    Result<void> handleStdin_(FdEventType type);
    Result<void> handleStdout_(FdEventType type);
    Result<void> handleStderr_(FdEventType type);

    Result<void> tryParseStdoutHeaders_();
    Result<void> fillStdinBufferIfNeeded_();
    void closeStdin_();
};

}  // namespace server

#endif
