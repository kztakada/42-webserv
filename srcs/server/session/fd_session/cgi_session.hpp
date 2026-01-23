#ifndef WEBSERV_CGI_SESSION_HPP_
#define WEBSERV_CGI_SESSION_HPP_

#include <functional>

#include "http/cgi_response.hpp"
#include "server/reactor/fd_event.hpp"
#include "server/session/fd/cgi_pipe/cgi_pipe_fd.hpp"
#include "server/session/fd_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "server/session/io_buffer.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;
using namespace http;

// CGIセッション：CGIプロセスとの通信状態を管理
class CgiSession : public FdSession
{
   public:
    static const long kDefaultTimeoutSec = 5;
    typedef std::function<void(CgiSession*)> CompletionCallback;

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
    HttpSession* parent_session_;      // 親となるHttpSessionへのポインタ
    FdSessionController& controller_;  // 監督者への参照

    // --- 状態管理 ---
    bool is_stdout_eof_;  // stdoutが閉じたか
    bool is_stderr_eof_;  // stderrが閉じたか

    bool input_complete_;
    bool output_complete_;
    bool process_finished_;

    virtual Result<void> onReadable();
    virtual Result<void> onWritable();
    virtual Result<void> onError();
    virtual Result<void> onTimeout();

    CompletionCallback completion_callback_;

   public:
    CgiSession(pid_t pid, int in_fd, int out_fd, int err_fd,
        HttpSession* parent, FdSessionController& controller)
        : FdSession(kDefaultTimeoutSec) {};
    virtual ~CgiSession();  // ここで確実に kill(pid_) と close を行う

    virtual Result<void> handleEvent(FdEvent* event, uint32_t triggered_events);
    virtual bool isComplete() const;

    HttpSession* getParentSession() const;

    void setCompletionCallback(CompletionCallback callback);

    bool isInputComplete() const;
    bool isOutputComplete() const;
    bool isProcessFinished() const;

    void setInputComplete(bool complete);
    void setOutputComplete(bool complete);

   private:
    CgiSession();
    CgiSession(const CgiSession& rhs);
    CgiSession& operator=(const CgiSession& rhs);

    void checkProcessStatus();
    void notifyCompletion();
};

}  // namespace server

#endif
