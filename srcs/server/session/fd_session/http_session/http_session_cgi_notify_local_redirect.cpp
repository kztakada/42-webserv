#include <unistd.h>

#include "http/cgi_response.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::handleCgiHeadersReadyLocalRedirect_(
    CgiSession& cgi, const http::CgiResponse& cr)
{
    if (redirect_count_ >= kMaxInternalRedirects)
    {
        // stdout は不要。詰まりを防ぐため閉じる。
        const int out_fd = cgi.releaseStdoutFd();
        if (out_fd >= 0)
            ::close(out_fd);

        if (active_cgi_session_ == &cgi)
        {
            controller_.requestDelete(active_cgi_session_);
            active_cgi_session_ = NULL;
        }

        RequestProcessor::Output out;
        Result<void> bo =
            buildErrorOutput_(http::HttpStatus::SERVER_ERROR, &out);
        if (bo.isError())
            return bo;
        response_.setHttpVersion(request_.getHttpVersion());

        installBodySourceAndWriter_(out.body_source);

        should_close_connection_ = should_close_connection_ || peer_closed_ ||
                                   out.should_close_connection ||
                                   !request_.shouldKeepAlive() ||
                                   handler_.shouldCloseConnection();
        changeState(new SendResponseState());
        (void)updateSocketWatches_();
        return Result<void>();
    }

    Result<std::string> loc = cr.getLocalRedirectUriPath();
    if (loc.isError())
        return Result<void>(ERROR, loc.getErrorMessage());

    // stdout は不要。以後 CGI が詰まらないように止める。
    const int out_fd = cgi.releaseStdoutFd();
    if (out_fd >= 0)
        ::close(out_fd);

    // いまの CGI は dispatch バッチ末尾で安全に delete する
    if (active_cgi_session_ == &cgi)
    {
        controller_.requestDelete(active_cgi_session_);
        active_cgi_session_ = NULL;
    }

    redirect_count_++;

    // internal redirect request を生成して再処理
    Result<http::HttpRequest> rr = buildInternalRedirectRequest_(loc.unwrap());
    if (rr.isError())
        return Result<void>(ERROR, rr.getErrorMessage());

    response_.reset();
    handler_.reset();
    request_ = rr.unwrap();

    // 古い CGI 待ち状態が残らないように一度クリアし、通常の経路へ委譲する。
    return prepareResponseOrCgi_();
}

}  // namespace server