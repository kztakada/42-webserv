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
#include "server/session/fd_session/http_session/session_context.hpp"
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

SessionCgiHandler::SessionCgiHandler() {}

SessionCgiHandler::~SessionCgiHandler() {}

Result<void> SessionCgiHandler::startCgi(HttpSession& session) {
    SessionContext& ctx = session.getContext();
    if (!ctx.request_handler.hasLocationRouting())
        return Result<void>(ERROR, "missing routing for CGI");

    Result<server::CgiContext> ctxr = ctx.request_handler.getLocationRouting().getCgiContext();
    if (ctxr.isError())
        return Result<void>(ERROR, ctxr.getErrorMessage());
    const server::CgiContext cgi_ctx = ctxr.unwrap();

    int request_body_fd = -1;
    if (ctx.request.hasBody()) {
        (void)ctx.request_handler.bodyStore().finish();
        Result<int> fd = ctx.request_handler.bodyStore().openForRead();
        if (fd.isOk())
            request_body_fd = fd.unwrap();
    }

    http::CgiMetaVariables meta = http::CgiMetaVariables::fromHttpRequest(
        ctx.request, cgi_ctx.script_name, cgi_ctx.path_info);

    meta.setServerName(ctx.socket_fd.getServerIp().toString());
    {
        std::istringstream iss(ctx.socket_fd.getServerPort().toString());
        unsigned int p = 0;
        iss >> p;
        meta.setServerPort(p);
    }
    meta.setRemoteAddr(ctx.socket_fd.getClientIp().toString());
    meta.setServerSoftware("webserv");

    std::map<std::string, std::string> env = meta.getAll();
    env["SCRIPT_FILENAME"] = cgi_ctx.script_filename.str();
    env["QUERY_STRING"] = cgi_ctx.query_string;
    if (isPhpCgiExecutor_(cgi_ctx.executor_path.str()))
        env["REDIRECT_STATUS"] = "200";

    std::vector<std::string> args;
    args.push_back(cgi_ctx.script_filename.str());

    const std::string working_dir = dirnameOf_(cgi_ctx.script_filename.str());
    Result<CgiSpawnResult> spawned = server::CgiPipeFd::Execute(
        cgi_ctx.executor_path.str(), args, env, working_dir);
    if (spawned.isError())
        return Result<void>(ERROR, spawned.getErrorMessage());

    const CgiSpawnResult s = spawned.unwrap();
    ctx.active_cgi_session = new CgiSession(s.pid, s.stdin_fd, s.stdout_fd,
        s.stderr_fd, request_body_fd, &session, ctx.controller);

    Result<void> d = ctx.controller.delegateSession(ctx.active_cgi_session);
    if (d.isError()) {
        delete ctx.active_cgi_session;
        ctx.active_cgi_session = NULL;
        return Result<void>(ERROR, d.getErrorMessage());
    }

    session.changeState(new ExecuteCgiState());
    (void)session.updateSocketWatches_();
    return Result<void>();
}

Result<void> SessionCgiHandler::onCgiHeadersReady(HttpSession& session, CgiSession& cgi) {
    if (!cgi.isHeadersComplete()) {
        return handleCgiError_(session, cgi, "CGI headers not complete despite notification");
    }
    const http::CgiResponse& cr = cgi.response();
    
    if (cr.getResponseType() == http::kLocalRedirect) {
        return handleCgiHeadersReadyLocalRedirect_(session, cgi, cr);
    }
    return handleCgiHeadersReadyNormal_(session, cgi, cr);
}

Result<void> SessionCgiHandler::onCgiError(HttpSession& session, CgiSession& cgi, const std::string& message) {
    return handleCgiError_(session, cgi, message);
}

Result<void> SessionCgiHandler::handleCgiError_(HttpSession& session, CgiSession& cgi, const std::string& message) {
    SessionContext& ctx = session.getContext();
    if (ctx.pending_state != NULL) return Result<void>();
    if (dynamic_cast<CloseWaitState*>(ctx.current_state)) return Result<void>();
    if (!dynamic_cast<ExecuteCgiState*>(ctx.current_state)) return Result<void>();

    utils::Log::error("CGI error: ", message);

    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd >= 0) ::close(stdout_fd);

    if (ctx.active_cgi_session == &cgi) {
        ctx.controller.requestDelete(ctx.active_cgi_session);
        ctx.active_cgi_session = NULL;
    } else {
        ctx.controller.requestDelete(&cgi);
    }

    ctx.response.reset();
    ctx.response.setHttpVersion(ctx.request.getHttpVersion());

    RequestProcessor::Output out;
    http::HttpStatus st = http::HttpStatus::BAD_GATEWAY;
    if (message.find("timeout") != std::string::npos) {
        st = http::HttpStatus::GATEWAY_TIMEOUT;
    }
    Result<void> bo = session.buildErrorOutput_(st, &out);
    if (bo.isError()) {
        out.body_source = NULL;
        out.should_close_connection = false;
    }
    ctx.response.setHttpVersion(ctx.request.getHttpVersion());

    session.installBodySourceAndWriter_(out.body_source);

    ctx.should_close_connection = ctx.should_close_connection || ctx.peer_closed ||
                               out.should_close_connection ||
                               !ctx.request.shouldKeepAlive() ||
                               ctx.request_handler.shouldCloseConnection();
    session.changeState(new SendResponseState());
    (void)session.updateSocketWatches_();
    return Result<void>();
}

Result<void> SessionCgiHandler::handleCgiHeadersReadyNormal_(HttpSession& session, CgiSession& cgi, const http::CgiResponse& cr) {
    SessionContext& ctx = session.getContext();
    ctx.response.reset();
    ctx.response.setHttpVersion(ctx.request.getHttpVersion());

    Result<void> ar = cr.applyToHttpResponse(ctx.response);
    if (ar.isError()) {
        const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
        (void)prefetched;
        const int stdout_fd = cgi.releaseStdoutFd();
        if (stdout_fd >= 0) ::close(stdout_fd);

        if (ctx.active_cgi_session == &cgi) {
            ctx.controller.requestDelete(ctx.active_cgi_session);
            ctx.active_cgi_session = NULL;
        }

        RequestProcessor::Output out;
        Result<void> bo = session.buildErrorOutput_(http::HttpStatus::BAD_GATEWAY, &out);
        if (bo.isError()) {
            out.body_source = NULL;
            out.should_close_connection = false;
        }
        ctx.response.setHttpVersion(ctx.request.getHttpVersion());

        session.installBodySourceAndWriter_(out.body_source);

        ctx.should_close_connection = ctx.should_close_connection || ctx.peer_closed ||
                                   out.should_close_connection ||
                                   !ctx.request.shouldKeepAlive() ||
                                   ctx.request_handler.shouldCloseConnection();
        session.changeState(new SendResponseState());
        (void)session.updateSocketWatches_();
        return Result<void>();
    }

    const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd < 0) return Result<void>(ERROR, "missing cgi stdout fd");

    BodySource* bs = new PrefetchedFdBodySource(stdout_fd, prefetched);
    session.installBodySourceAndWriter_(bs);
    session.changeState(new SendResponseState());
    (void)session.updateSocketWatches_();

    if (ctx.active_cgi_session == &cgi) ctx.active_cgi_session = NULL;
    return Result<void>();
}

Result<void> SessionCgiHandler::handleCgiHeadersReadyLocalRedirect_(HttpSession& session, CgiSession& cgi, const http::CgiResponse& cr) {
    SessionContext& ctx = session.getContext();
    if (ctx.redirect_count >= HttpSession::kMaxInternalRedirects) {
        const int out_fd = cgi.releaseStdoutFd();
        if (out_fd >= 0) ::close(out_fd);

        if (ctx.active_cgi_session == &cgi) {
            ctx.controller.requestDelete(ctx.active_cgi_session);
            ctx.active_cgi_session = NULL;
        }

        RequestProcessor::Output out;
        Result<void> bo = session.buildErrorOutput_(http::HttpStatus::SERVER_ERROR, &out);
        if (bo.isError()) return bo;
        ctx.response.setHttpVersion(ctx.request.getHttpVersion());

        session.installBodySourceAndWriter_(out.body_source);

        ctx.should_close_connection = ctx.should_close_connection || ctx.peer_closed ||
                                   out.should_close_connection ||
                                   !ctx.request.shouldKeepAlive() ||
                                   ctx.request_handler.shouldCloseConnection();
        session.changeState(new SendResponseState());
        (void)session.updateSocketWatches_();
        return Result<void>();
    }

    Result<std::string> loc = cr.getLocalRedirectUriPath();
    if (loc.isError()) return Result<void>(ERROR, loc.getErrorMessage());

    const int out_fd = cgi.releaseStdoutFd();
    if (out_fd >= 0) ::close(out_fd);

    if (ctx.active_cgi_session == &cgi) {
        ctx.controller.requestDelete(ctx.active_cgi_session);
        ctx.active_cgi_session = NULL;
    }

    ctx.redirect_count++;

    Result<http::HttpRequest> rr = session.buildInternalRedirectRequest_(loc.unwrap());
    if (rr.isError()) return Result<void>(ERROR, rr.getErrorMessage());

    ctx.response.reset();
    ctx.request_handler.reset();
    ctx.request = rr.unwrap();

    return session.prepareResponseOrCgi_();
}

} // namespace server