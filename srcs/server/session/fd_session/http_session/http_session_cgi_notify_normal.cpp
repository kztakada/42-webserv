#include <unistd.h>

#include "http/cgi_response.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::handleCgiHeadersReadyNormal_(
    CgiSession& cgi, const http::CgiResponse& cr)
{
    // 通常: CGI ヘッダを HTTP Response に適用して、body は stdout を流す
    response_.reset();
    response_.setHttpVersion(request_.getHttpVersion());

    Result<void> ar = cr.applyToHttpResponse(response_);
    if (ar.isError())
    {
        // CGIヘッダが壊れている場合は upstream の不正応答として 502 を返す。
        const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
        (void)prefetched;
        const int stdout_fd = cgi.releaseStdoutFd();
        if (stdout_fd >= 0)
            ::close(stdout_fd);

        if (active_cgi_session_ == &cgi)
        {
            controller_.requestDelete(active_cgi_session_);
            active_cgi_session_ = NULL;
        }

        RequestProcessor::Output out;
        Result<void> bo =
            buildErrorOutput_(http::HttpStatus::BAD_GATEWAY, &out);
        if (bo.isError())
        {
            out.body_source = NULL;
            out.should_close_connection = false;
        }
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

    const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd < 0)
    {
        return Result<void>(ERROR, "missing cgi stdout fd");
    }

    BodySource* bs = new PrefetchedFdBodySource(stdout_fd, prefetched);
    installBodySourceAndWriter_(bs);
    changeState(new SendResponseState());
    (void)updateSocketWatches_();

    // 以後 CgiSession は controller が所有/削除する。HttpSession
    // 側では触らない。
    if (active_cgi_session_ == &cgi)
        active_cgi_session_ = NULL;
    return Result<void>();
}

}  // namespace server