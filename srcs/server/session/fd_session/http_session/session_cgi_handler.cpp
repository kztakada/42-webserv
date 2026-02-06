#include "server/session/fd_session/http_session/session_cgi_handler.hpp"

#include <unistd.h>
#include <map>
#include <sstream>
#include <vector>

#include "http/cgi_meta_variables.hpp"
#include "http/cgi_response.hpp"
#include "server/session/fd/cgi_pipe/cgi_pipe_fd.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "utils/log.hpp"

namespace server {

using namespace utils::result;

static std::string dirnameOf_(const std::string& path) {
    const std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos) return std::string();
    if (pos == 0) return std::string("/");
    return path.substr(0, pos);
}

static bool isPhpCgiExecutor_(const std::string& executor_path) {
    const std::string::size_type pos = executor_path.find_last_of('/');
    const std::string base = (pos == std::string::npos) ? executor_path : executor_path.substr(pos + 1);
    return base.find("php-cgi") != std::string::npos;
}

SessionCgiHandler::SessionCgiHandler(HttpSession& session)
    : session_(session), active_cgi_session_(NULL) {}

SessionCgiHandler::~SessionCgiHandler() {
    active_cgi_session_ = NULL;
}

CgiSession* SessionCgiHandler::getActiveCgiSession() const {
    return active_cgi_session_;
}

void SessionCgiHandler::clearActiveCgiSession() {
    active_cgi_session_ = NULL;
}

Result<void> SessionCgiHandler::startCgi() {
    if (!session_.handler_.hasLocationRouting())
        return Result<void>(ERROR, "missing routing for CGI");

    Result<server::CgiContext> ctxr = session_.handler_.getLocationRouting().getCgiContext();
    if (ctxr.isError())
        return Result<void>(ERROR, ctxr.getErrorMessage());
    const server::CgiContext ctx = ctxr.unwrap();

    int request_body_fd = -1;
    if (session_.request_.hasBody()) {
        (void)session_.handler_.bodyStore().finish();
        Result<int> fd = session_.handler_.bodyStore().openForRead();
        if (fd.isOk())
            request_body_fd = fd.unwrap();
    }

    http::CgiMetaVariables meta = http::CgiMetaVariables::fromHttpRequest(
        session_.request_, ctx.script_name, ctx.path_info);

    meta.setServerName(session_.socket_fd_.getServerIp().toString());
    {
        std::istringstream iss(session_.socket_fd_.getServerPort().toString());
        unsigned int p = 0;
        iss >> p;
        meta.setServerPort(p);
    }
    meta.setRemoteAddr(session_.socket_fd_.getClientIp().toString());
    meta.setServerSoftware("webserv");

    std::map<std::string, std::string> env = meta.getAll();
    env["SCRIPT_FILENAME"] = ctx.script_filename.str();
    env["QUERY_STRING"] = ctx.query_string;
    if (isPhpCgiExecutor_(ctx.executor_path.str()))
        env["REDIRECT_STATUS"] = "200";

    std::vector<std::string> args;
    args.push_back(ctx.script_filename.str());

    const std::string working_dir = dirnameOf_(ctx.script_filename.str());
    Result<CgiSpawnResult> spawned = server::CgiPipeFd::Execute(
        ctx.executor_path.str(), args, env, working_dir);
    if (spawned.isError())
        return Result<void>(ERROR, spawned.getErrorMessage());

    const CgiSpawnResult s = spawned.unwrap();
    active_cgi_session_ = new CgiSession(s.pid, s.stdin_fd, s.stdout_fd,
        s.stderr_fd, request_body_fd, &session_, session_.controller_);

    Result<void> d = session_.controller_.delegateSession(active_cgi_session_);
    if (d.isError()) {
        delete active_cgi_session_;
        active_cgi_session_ = NULL;
        return Result<void>(ERROR, d.getErrorMessage());
    }

    session_.changeState(new ExecuteCgiState());
    (void)session_.updateSocketWatches_();
    return Result<void>();
}

Result<void> SessionCgiHandler::onCgiHeadersReady(CgiSession& cgi) {
    if (!cgi.isHeadersComplete()) {
        return handleCgiError_(cgi, "CGI headers not complete despite notification");
    }
    const http::CgiResponse& cr = cgi.response();
    
    if (cr.getResponseType() == http::kLocalRedirect) {
        return handleCgiHeadersReadyLocalRedirect_(cgi, cr);
    }
    return handleCgiHeadersReadyNormal_(cgi, cr);
}

Result<void> SessionCgiHandler::onCgiError(CgiSession& cgi, const std::string& message) {
    return handleCgiError_(cgi, message);
}

Result<void> SessionCgiHandler::handleCgiError_(CgiSession& cgi, const std::string& message) {
    if (session_.pending_state_ != NULL) return Result<void>();
    if (dynamic_cast<CloseWaitState*>(session_.current_state_)) return Result<void>();
    if (!dynamic_cast<ExecuteCgiState*>(session_.current_state_)) return Result<void>();

    utils::Log::error("CGI error: ", message);

    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd >= 0) ::close(stdout_fd);

    if (active_cgi_session_ == &cgi) {
        session_.controller_.requestDelete(active_cgi_session_);
        active_cgi_session_ = NULL;
    } else {
        session_.controller_.requestDelete(&cgi);
    }

    session_.response_.reset();
    session_.response_.setHttpVersion(session_.request_.getHttpVersion());

    RequestProcessor::Output out;
    http::HttpStatus st = http::HttpStatus::BAD_GATEWAY;
    if (message.find("timeout") != std::string::npos) {
        st = http::HttpStatus::GATEWAY_TIMEOUT;
    }
    Result<void> bo = session_.buildErrorOutput_(st, &out);
    if (bo.isError()) {
        out.body_source = NULL;
        out.should_close_connection = false;
    }
    session_.response_.setHttpVersion(session_.request_.getHttpVersion());

    session_.installBodySourceAndWriter_(out.body_source);

    session_.should_close_connection_ = session_.should_close_connection_ || session_.peer_closed_ ||
                               out.should_close_connection ||
                               !session_.request_.shouldKeepAlive() ||
                               session_.handler_.shouldCloseConnection();
    session_.changeState(new SendResponseState());
    (void)session_.updateSocketWatches_();
    return Result<void>();
}

Result<void> SessionCgiHandler::handleCgiHeadersReadyNormal_(CgiSession& cgi, const http::CgiResponse& cr) {
    session_.response_.reset();
    session_.response_.setHttpVersion(session_.request_.getHttpVersion());

    Result<void> ar = cr.applyToHttpResponse(session_.response_);
    if (ar.isError()) {
        const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
        (void)prefetched;
        const int stdout_fd = cgi.releaseStdoutFd();
        if (stdout_fd >= 0) ::close(stdout_fd);

        if (active_cgi_session_ == &cgi) {
            session_.controller_.requestDelete(active_cgi_session_);
            active_cgi_session_ = NULL;
        }

        RequestProcessor::Output out;
        Result<void> bo = session_.buildErrorOutput_(http::HttpStatus::BAD_GATEWAY, &out);
        if (bo.isError()) {
            out.body_source = NULL;
            out.should_close_connection = false;
        }
        session_.response_.setHttpVersion(session_.request_.getHttpVersion());

        session_.installBodySourceAndWriter_(out.body_source);

        session_.should_close_connection_ = session_.should_close_connection_ || session_.peer_closed_ ||
                                   out.should_close_connection ||
                                   !session_.request_.shouldKeepAlive() ||
                                   session_.handler_.shouldCloseConnection();
        session_.changeState(new SendResponseState());
        (void)session_.updateSocketWatches_();
        return Result<void>();
    }

    const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd < 0) return Result<void>(ERROR, "missing cgi stdout fd");

    BodySource* bs = new PrefetchedFdBodySource(stdout_fd, prefetched);
    session_.installBodySourceAndWriter_(bs);
    session_.changeState(new SendResponseState());
    (void)session_.updateSocketWatches_();

    if (active_cgi_session_ == &cgi) active_cgi_session_ = NULL;
    return Result<void>();
}

Result<void> SessionCgiHandler::handleCgiHeadersReadyLocalRedirect_(CgiSession& cgi, const http::CgiResponse& cr) {
    if (session_.redirect_count_ >= HttpSession::kMaxInternalRedirects) {
        const int out_fd = cgi.releaseStdoutFd();
        if (out_fd >= 0) ::close(out_fd);

        if (active_cgi_session_ == &cgi) {
            session_.controller_.requestDelete(active_cgi_session_);
            active_cgi_session_ = NULL;
        }

        RequestProcessor::Output out;
        Result<void> bo = session_.buildErrorOutput_(http::HttpStatus::SERVER_ERROR, &out);
        if (bo.isError()) return bo;
        session_.response_.setHttpVersion(session_.request_.getHttpVersion());

        session_.installBodySourceAndWriter_(out.body_source);

        session_.should_close_connection_ = session_.should_close_connection_ || session_.peer_closed_ ||
                                   out.should_close_connection ||
                                   !session_.request_.shouldKeepAlive() ||
                                   session_.handler_.shouldCloseConnection();
        session_.changeState(new SendResponseState());
        (void)session_.updateSocketWatches_();
        return Result<void>();
    }

    Result<std::string> loc = cr.getLocalRedirectUriPath();
    if (loc.isError()) return Result<void>(ERROR, loc.getErrorMessage());

    const int out_fd = cgi.releaseStdoutFd();
    if (out_fd >= 0) ::close(out_fd);

    if (active_cgi_session_ == &cgi) {
        session_.controller_.requestDelete(active_cgi_session_);
        active_cgi_session_ = NULL;
    }

    session_.redirect_count_++;

    Result<http::HttpRequest> rr = session_.buildInternalRedirectRequest_(loc.unwrap());
    if (rr.isError()) return Result<void>(ERROR, rr.getErrorMessage());

    session_.response_.reset();
    session_.handler_.reset();
    session_.request_ = rr.unwrap();

    return session_.prepareResponseOrCgi_();
}

} // namespace server