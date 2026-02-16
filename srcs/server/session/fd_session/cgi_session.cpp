#include "server/session/fd_session/cgi_session.hpp"

#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#include "server/session/fd_session/http_session.hpp"
#include "utils/data_type.hpp"
#include "utils/log.hpp"

namespace server
{

CgiSession::CgiSession(pid_t pid, int in_fd, int out_fd, int err_fd,
    int request_body_fd, HttpSession* parent, FdSessionController& controller,
    utils::ProcessingLog* processing_log)
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
      processing_log_(processing_log),
      is_counted_as_active_cgi_(false),
      request_body_fd_(request_body_fd),
      is_stdout_eof_(false),
      is_stderr_eof_(false),
      input_complete_(false),
      headers_complete_(false),
      error_notified_to_parent_(false),
      prefetched_body_()
{
    updateLastActiveTime();
}

std::vector<utils::Byte> CgiSession::takePrefetchedBody()
{
    std::vector<utils::Byte> out;
    out.swap(prefetched_body_);
    return out;
}

CgiSession::~CgiSession()
{
    if (processing_log_ != NULL && is_counted_as_active_cgi_)
        processing_log_->onCgiFinished();  // ログ計測

    // reactor watch を残したまま delete すると UAF になるため、先に解除する。
    if (pipe_in_.getFd() >= 0)
        controller_.unregisterFd(pipe_in_.getFd());
    if (pipe_out_.getFd() >= 0)
        controller_.unregisterFd(pipe_out_.getFd());
    if (pipe_err_.getFd() >= 0)
        controller_.unregisterFd(pipe_err_.getFd());

    if (request_body_fd_ >= 0)
    {
        ::close(request_body_fd_);
        request_body_fd_ = -1;
    }

    // プロセス回収（ブロックしない範囲で）
    if (pid_ > 0)
    {
        int status = 0;
        pid_t r = ::waitpid(pid_, &status, WNOHANG);
        if (r == 0)
        {
            if (controller_.isShuttingDown())
            {
                // 親プロセス(shutdown)中は、まずSIGTERMで終了を促す
                (void)::kill(pid_, SIGTERM);

                // 短い猶予の間だけ終了を待つ（sleep系は使わず select で待機）
                const int kGraceTotalMs = 300;
                const int kTickMs = 30;
                int waited_ms = 0;
                while (waited_ms < kGraceTotalMs)
                {
                    r = ::waitpid(pid_, &status, WNOHANG);
                    if (r != 0)
                        break;

                    struct timeval tv;
                    tv.tv_sec = 0;
                    tv.tv_usec = kTickMs * 1000;
                    (void)::select(0, NULL, NULL, NULL, &tv);
                    waited_ms += kTickMs;
                }
            }

            // まだ生きていれば強制終了（従来通り）
            r = ::waitpid(pid_, &status, WNOHANG);
            if (r == 0)
            {
                (void)::kill(pid_, SIGKILL);
                (void)::waitpid(pid_, &status, 0);
            }
        }
        pid_ = -1;
    }
}

// ログ計測
void CgiSession::markCountedAsActiveCgi()
{
    if (processing_log_ == NULL)
        return;
    if (is_counted_as_active_cgi_)
        return;
    is_counted_as_active_cgi_ = true;
    processing_log_->onCgiStarted();
}

bool CgiSession::isComplete() const
{
    // 親(HttpSession)が CGI の stdout をストリーミングしている間は、
    // この CgiSession を self-complete させない。
    // （親がレスポンス完了時に明示的に requestDelete する）
    if (parent_session_ != NULL)
        return false;

    // headers 完了後は stdout を親へ移譲する想定。
    // ここでは最低限、stdin が閉じて stderr も EOF になったら complete 扱い。
    if (!headers_complete_)
        return false;

    if (!input_complete_)
        return false;

    if (!is_stderr_eof_)
        return false;

    return true;
}

void CgiSession::getInitialWatchSpecs(
    std::vector<FdSession::FdWatchSpec>* out) const
{
    if (out == NULL)
        return;
    if (pipe_in_.getFd() >= 0)
        out->push_back(FdSession::FdWatchSpec(pipe_in_.getFd(), false, true));
    if (pipe_out_.getFd() >= 0)
        out->push_back(FdSession::FdWatchSpec(pipe_out_.getFd(), true, false));
    if (pipe_err_.getFd() >= 0)
        out->push_back(FdSession::FdWatchSpec(pipe_err_.getFd(), true, false));
}

Result<void> CgiSession::handleEvent(const FdEvent& event)
{
    updateLastActiveTime();
    if (parent_session_ != NULL)
        parent_session_->updateLastActiveTime();

    if (event.type == kTimeoutEvent || event.type == kErrorEvent)
    {
        const char* msg = (event.type == kTimeoutEvent) ? "cgi session timeout"
                                                        : "cgi session error";

        // 親側は CGI エラーとして扱う想定
        if (parent_session_ != NULL && !error_notified_to_parent_)
        {
            error_notified_to_parent_ = true;
            (void)parent_session_->onCgiError(*this, msg);
        }
        controller_.requestDelete(this);
        return Result<void>(ERROR, msg);
    }

    Result<void> r;
    if (pipe_in_.isSame(event.fd))
        r = handleStdin_(event.type);
    else if (pipe_out_.isSame(event.fd))
        r = handleStdout_(event.type);
    else if (pipe_err_.isSame(event.fd))
        r = handleStderr_(event.type);
    else
        r = Result<void>(ERROR, "Unknown FD in CgiSession");

    if (r.isError())
    {
        if (parent_session_ != NULL && !error_notified_to_parent_)
        {
            error_notified_to_parent_ = true;
            (void)parent_session_->onCgiError(*this, r.getErrorMessage());
        }
        controller_.requestDelete(this);
        return r;
    }

    if (isComplete())
        controller_.requestDelete(this);

    return r;
}

Result<void> CgiSession::fillStdinBufferIfNeeded_()
{
    if (request_body_fd_ < 0)
        return Result<void>();

    // stdin_buffer_ が空に近い時だけ読み足す
    if (stdin_buffer_.size() >= utils::kPageSizeMin)
        return Result<void>();

    char buf[utils::kPageSizeMin];
    const ssize_t n = ::read(request_body_fd_, buf, sizeof(buf));
    if (n < 0)
        return Result<void>(ERROR, "internal fd read failed");

    if (n == 0)
    {
        ::close(request_body_fd_);
        request_body_fd_ = -1;
        return Result<void>();
    }

    stdin_buffer_.append(buf, static_cast<size_t>(n));
    return Result<void>();
}

void CgiSession::closeStdin_()
{
    if (pipe_in_.getFd() >= 0)
    {
        controller_.unregisterFd(pipe_in_.getFd());
        const int fd = pipe_in_.release();
        if (fd >= 0)
            ::close(fd);
    }
    input_complete_ = true;
}

Result<void> CgiSession::handleStdin_(FdEventType type)
{
    if (type != kWriteEvent)
        return Result<void>();

    if (input_complete_)
        return Result<void>();

    Result<void> f = fillStdinBufferIfNeeded_();
    if (f.isError())
        return f;

    if (stdin_buffer_.size() > 0)
    {
        const ssize_t w = stdin_buffer_.flushToFd(pipe_in_.getFd());
        if (w < 0)
        {
            if (processing_log_ != NULL)
                processing_log_->incrementBlockIo();  // ログ計測
            return Result<void>();
        }
    }

    // 送るものがなくなり、body の読み元も EOF なら stdin を閉じる
    if (stdin_buffer_.size() == 0 && request_body_fd_ < 0)
        closeStdin_();

    return Result<void>();
}

Result<void> CgiSession::tryParseStdoutHeaders_()
{
    if (headers_complete_)
        return Result<void>();

    const size_t nbytes = stdout_buffer_.size();
    if (nbytes == 0)
        return Result<void>();

    const utils::Byte* data =
        reinterpret_cast<const utils::Byte*>(stdout_buffer_.data());

    Result<size_t> consumed = cgi_response_.parse(data, nbytes);
    if (consumed.isError())
        return Result<void>(ERROR, consumed.getErrorMessage());

    const size_t c = consumed.unwrap();
    if (c > 0)
        stdout_buffer_.consume(c);

    if (!cgi_response_.isParseComplete())
        return Result<void>();

    // 残りは body として先読み分に退避
    if (stdout_buffer_.size() > 0)
    {
        const size_t remain = stdout_buffer_.size();
        const utils::Byte* p =
            reinterpret_cast<const utils::Byte*>(stdout_buffer_.data());
        prefetched_body_.insert(prefetched_body_.end(), p, p + remain);
        stdout_buffer_.consume(remain);
    }

    headers_complete_ = true;

    // 以降 stdout は親が読むため、watch を外す。
    if (pipe_out_.getFd() >= 0)
        controller_.unregisterFd(pipe_out_.getFd());

    // 親へ通知（レスポンス送信準備）
    if (parent_session_ != NULL)
    {
        Result<void> n = parent_session_->onCgiHeadersReady(*this);
        if (n.isError())
            return Result<void>(ERROR, n.getErrorMessage());
    }
    return Result<void>();
}

Result<void> CgiSession::handleStdout_(FdEventType type)
{
    if (type != kReadEvent)
        return Result<void>();

    // headers 完了後の stdout は親が BodySource として読む。
    if (headers_complete_)
        return Result<void>();

    const ssize_t r = stdout_buffer_.fillFromFd(pipe_out_.getFd());

    if (r < 0)
    {
        if (processing_log_ != NULL)
            processing_log_->incrementBlockIo();  // ログ計測
        return Result<void>();
    }

    if (r == 0)
    {
        is_stdout_eof_ = true;
        // EOF でも、ヘッダが完了していなければエラー
        if (!headers_complete_)
            return Result<void>(ERROR, "cgi stdout closed before headers");
        return Result<void>();
    }

    return tryParseStdoutHeaders_();
}

Result<void> CgiSession::handleStderr_(FdEventType type)
{
    if (type != kReadEvent)
        return Result<void>();

    const ssize_t r = stderr_buffer_.fillFromFd(pipe_err_.getFd());
    if (r < 0)
    {
        if (processing_log_ != NULL)
            processing_log_->incrementBlockIo();  // ログ計測
        return Result<void>();
    }

    if (r == 0)
    {
        is_stderr_eof_ = true;
        if (pipe_err_.getFd() >= 0)
            controller_.unregisterFd(pipe_err_.getFd());
        return Result<void>();
    }

    // stderr はログに出す（大量なら本来制限したい）
    if (stderr_buffer_.size() > 0)
    {
        utils::Log::error("CgiSession", "CGI stderr:",
            std::string(stderr_buffer_.data(),
                stderr_buffer_.data() + stderr_buffer_.size()));
        stderr_buffer_.consume(stderr_buffer_.size());
    }

    return Result<void>();
}

}  // namespace server
