#include "server/session/fd_session/cgi_session.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "server/session/fd_session/http_session.hpp"
#include "utils/log.hpp"

namespace server
{

CgiSession::~CgiSession()
{
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
        const pid_t r = ::waitpid(pid_, &status, WNOHANG);
        if (r == 0)
        {
            ::kill(pid_, SIGKILL);
            ::waitpid(pid_, &status, 0);
        }
        pid_ = -1;
    }
}

bool CgiSession::isComplete() const
{
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

Result<void> CgiSession::handleEvent(const FdEvent& event)
{
    updateLastActiveTime();

    if (event.type == kTimeoutEvent || event.type == kErrorEvent)
    {
        // 親側は CGI エラーとして扱う想定
        return Result<void>(ERROR, "cgi session error/timeout");
    }

    if (pipe_in_.isSame(event.fd))
        return handleStdin_(event.type);
    if (pipe_out_.isSame(event.fd))
        return handleStdout_(event.type);
    if (pipe_err_.isSame(event.fd))
        return handleStderr_(event.type);

    return Result<void>(ERROR, "Unknown FD in CgiSession");
}

Result<void> CgiSession::fillStdinBufferIfNeeded_()
{
    if (request_body_fd_ < 0)
        return Result<void>();

    // stdin_buffer_ が空に近い時だけ読み足す
    if (stdin_buffer_.size() >= 4096)
        return Result<void>();

    char buf[4096];
    const ssize_t n = ::read(request_body_fd_, buf, sizeof(buf));
    if (n < 0)
    {
        // 実装規定: read 後の errno 分岐は禁止。
        // -1 は "今は進めない" として扱う。
        return Result<void>();
    }

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
            // 実装規定: write 後の errno 分岐は禁止。
            return Result<void>();
        }
    }

    // 送るものがなくなり、body の読み元も EOF なら stdin を閉じる
    if (stdin_buffer_.size() == 0 && request_body_fd_ < 0)
    {
        closeStdin_();
    }

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

    if (headers_complete_)
    {
        // headers 完了後の stdout は親が BodySource として読む。
        return Result<void>();
    }

    const ssize_t r = stdout_buffer_.fillFromFd(pipe_out_.getFd());
    if (r < 0)
    {
        // 実装規定: read 後の errno 分岐は禁止。
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
        utils::Log::error(
            "CGI stderr: ", std::string(stderr_buffer_.data(),
                                stderr_buffer_.data() + stderr_buffer_.size()));
        stderr_buffer_.consume(stderr_buffer_.size());
    }

    return Result<void>();
}

}  // namespace server
