#include "server/http_processing_module/session_cgi_handler.hpp"

#include <unistd.h>

#include <ctime>
#include <map>
#include <sstream>
#include <vector>

#include "http/cgi_meta_variables.hpp"
#include "http/cgi_response.hpp"
#include "server/session/fd/cgi_pipe/cgi_pipe_fd.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/session_context.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session_controller.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

static std::string dirnameOf_(const std::string& path)
{
    const std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return std::string();
    if (pos == 0)
        return std::string("/");
    return path.substr(0, pos);
}

static bool isPhpCgiExecutor_(const std::string& executor_path)
{
    const std::string::size_type pos = executor_path.find_last_of('/');
    const std::string base = (pos == std::string::npos)
                                 ? executor_path
                                 : executor_path.substr(pos + 1);
    return base.find("php-cgi") != std::string::npos;
}

SessionCgiHandler::SessionCgiHandler(FdSessionController& controller)
    : controller_(controller)
{
}

SessionCgiHandler::~SessionCgiHandler() {}

Result<void> SessionCgiHandler::startCgi(HttpSession& session)
{
    SessionContext& ctx = session.getContext();
    if (!ctx.request_handler.hasLocationRouting())
        return Result<void>(ERROR, "missing routing for CGI");

    Result<server::CgiContext> ctxr =
        ctx.request_handler.getLocationRouting().getCgiContext();
    if (ctxr.isError())
        return Result<void>(ERROR, ctxr.getErrorMessage());
    const server::CgiContext cgi_ctx = ctxr.unwrap();

    int request_body_fd = -1;
    if (ctx.request.hasBody())
    {
        (void)ctx.request_handler.bodyStore().finish();
        Result<int> fd = ctx.request_handler.bodyStore().openForRead();
        if (fd.isOk())
            request_body_fd = fd.unwrap();
    }

    // cgi_extension は「拡張子に応じて特定の実行プログラムを起動する」ため、
    // SCRIPT_NAME は URI 上の CGI スクリプトを指さない（実行プログラム自体は
    // URI に現れない）。 そのため、PATH_INFO に request path 全体（query
    // を除く）を入れる。
    // - ルーティングで分解した script_name + path_info は、query を除いた
    // request path 全体になる。
    const std::string full_request_path =
        cgi_ctx.script_name + cgi_ctx.path_info;
    http::CgiMetaVariables meta = http::CgiMetaVariables::fromHttpRequest(
        ctx.request, "", full_request_path);

    meta.setServerName(ctx.socket_fd.getServerIp().toString());
    meta.setServerPort(ctx.socket_fd.getServerPort().toNumber());

    meta.setRemoteAddr(ctx.socket_fd.getClientIp().toString());
    meta.setServerSoftware("webserv");

    std::map<std::string, std::string> env = meta.getAll();
    env["SCRIPT_FILENAME"] = cgi_ctx.script_filename.str();
    env["QUERY_STRING"] = cgi_ctx.query_string;
    if (isPhpCgiExecutor_(cgi_ctx.executor_path.str()))
        env["REDIRECT_STATUS"] = "200";

    // PATH_INFO / SCRIPT_NAME は CgiMetaVariables 側で設定済み。

    std::vector<std::string> args;
    args.push_back(cgi_ctx.script_filename.str());

    const std::string working_dir = dirnameOf_(cgi_ctx.script_filename.str());
    Result<CgiSpawnResult> spawned = server::CgiPipeFd::Execute(
        cgi_ctx.executor_path.str(), args, env, working_dir);
    if (spawned.isError())
        return Result<void>(ERROR, spawned.getErrorMessage());

    const CgiSpawnResult s = spawned.unwrap();
    ctx.active_cgi_session =
        new CgiSession(s.pid, s.stdin_fd, s.stdout_fd, s.stderr_fd,
            request_body_fd, &session, controller_, session.processingLog());

    Result<void> d = controller_.delegateSession(ctx.active_cgi_session);
    if (d.isError())
    {
        delete ctx.active_cgi_session;
        ctx.active_cgi_session = NULL;
        return Result<void>(ERROR, d.getErrorMessage());
    }

    if (ctx.active_cgi_session != NULL)
        ctx.active_cgi_session->markCountedAsActiveCgi();  // ログ計測

    session.changeState(new ExecuteCgiState());
    (void)session.updateSocketWatches_();
    return Result<void>();
}

Result<void> SessionCgiHandler::onCgiHeadersReady(
    HttpSession& session, CgiSession& cgi)
{
    if (!cgi.isHeadersComplete())
    {
        return handleCgiError_(
            session, cgi, "CGI headers not complete despite notification");
    }
    const http::CgiResponse& cr = cgi.response();

    if (cr.getResponseType() == http::kLocalRedirect)
    {
        return handleCgiHeadersReadyLocalRedirect_(session, cgi, cr);
    }
    return handleCgiHeadersReadyNormal_(session, cgi, cr);
}

Result<void> SessionCgiHandler::onCgiError(
    HttpSession& session, CgiSession& cgi, const std::string& message)
{
    return handleCgiError_(session, cgi, message);
}

Result<void> SessionCgiHandler::handleCgiError_(
    HttpSession& session, CgiSession& cgi, const std::string& message)
{
    SessionContext& ctx = session.getContext();
    if (dynamic_cast<CloseWaitState*>(ctx.current_state))
        return Result<void>();

    bool can_handle =
        (dynamic_cast<ExecuteCgiState*>(ctx.current_state) != NULL);
    if (!can_handle)
    {
        // headers 完了後に body が来ないまま timeout したケースを拾う。
        // この場合は response_writer が未作成（ヘッダ未送出）なので 504
        // にできる。
        if (dynamic_cast<SendResponseState*>(ctx.current_state) != NULL &&
            ctx.response_writer == NULL && ctx.cgi_stdout_fd_for_response >= 0)
        {
            can_handle = true;
        }
    }
    if (!can_handle)
        return Result<void>();

    utils::Log::error("SessionCgiHandler", "CGI error:", message);

    // CgiSession 側が stdout を保持していれば回収、すでに親へ移譲済みなら -1
    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd >= 0)
        ::close(stdout_fd);

    // headers 完了後に stdout を親が保持している場合は、ここで解除・破棄する。
    if (ctx.cgi_stdout_fd_for_response >= 0)
    {
        session.clearBodyWatch_();
        controller_.unregisterFd(ctx.cgi_stdout_fd_for_response);
        ::close(ctx.cgi_stdout_fd_for_response);
        ctx.cgi_stdout_fd_for_response = -1;
    }
    ctx.cgi_prefetched_body.clear();
    ctx.waiting_cgi_first_body = false;
    ctx.waiting_cgi_first_body_start = 0;
    ctx.pause_write_until_body_ready = false;

    if (ctx.active_cgi_session == &cgi)
    {
        controller_.requestDelete(ctx.active_cgi_session);
        ctx.active_cgi_session = NULL;
    }
    else
    {
        controller_.requestDelete(&cgi);
    }

    ctx.response.reset();
    ctx.response.setHttpVersion(ctx.request.getHttpVersion());

    RequestProcessor::Output out;
    http::HttpStatus st = http::HttpStatus::BAD_GATEWAY;
    if (message.find("timeout") != std::string::npos)
    {
        st = http::HttpStatus::GATEWAY_TIMEOUT;
    }
    Result<void> bo = session.buildErrorOutput_(st, &out);
    if (bo.isError())
    {
        out.body_source.reset(NULL);
        out.should_close_connection = false;
    }
    ctx.response.setHttpVersion(ctx.request.getHttpVersion());

    session.installBodySourceAndWriter_(out.body_source);

    ctx.should_close_connection =
        ctx.should_close_connection || ctx.peer_closed ||
        out.should_close_connection || !ctx.request.shouldKeepAlive() ||
        ctx.request_handler.shouldCloseConnection();
    session.changeState(new SendResponseState());
    (void)session.updateSocketWatches_();
    return Result<void>();
}

Result<void> SessionCgiHandler::handleCgiHeadersReadyNormal_(
    HttpSession& session, CgiSession& cgi, const http::CgiResponse& cr)
{
    SessionContext& ctx = session.getContext();
    ctx.response.reset();
    ctx.response.setHttpVersion(ctx.request.getHttpVersion());

    Result<void> ar = cr.applyToHttpResponse(ctx.response);
    if (ar.isError())
    {
        const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
        (void)prefetched;
        const int stdout_fd = cgi.releaseStdoutFd();
        if (stdout_fd >= 0)
            ::close(stdout_fd);

        if (ctx.active_cgi_session == &cgi)
        {
            controller_.requestDelete(ctx.active_cgi_session);
            ctx.active_cgi_session = NULL;
        }

        RequestProcessor::Output out;
        Result<void> bo =
            session.buildErrorOutput_(http::HttpStatus::BAD_GATEWAY, &out);
        if (bo.isError())
        {
            out.body_source.reset(NULL);
            out.should_close_connection = false;
        }
        ctx.response.setHttpVersion(ctx.request.getHttpVersion());

        session.installBodySourceAndWriter_(out.body_source);

        ctx.should_close_connection =
            ctx.should_close_connection || ctx.peer_closed ||
            out.should_close_connection || !ctx.request.shouldKeepAlive() ||
            ctx.request_handler.shouldCloseConnection();
        session.changeState(new SendResponseState());
        (void)session.updateSocketWatches_();
        return Result<void>();
    }

    const std::vector<utils::Byte> prefetched = cgi.takePrefetchedBody();
    const int stdout_fd = cgi.releaseStdoutFd();
    if (stdout_fd < 0)
        return Result<void>(ERROR, "missing cgi stdout fd");

    // まずは stdout を watch し、最初の body byte
    // が来るまでヘッダ送出を遅延する。 これにより「ヘッダ確定後に body
    // が来ないまま timeout」した場合に 504 を返せる。
    ctx.cgi_stdout_fd_for_response = stdout_fd;
    ctx.cgi_prefetched_body = prefetched;
    ctx.waiting_cgi_first_body = prefetched.empty();
    ctx.waiting_cgi_first_body_start = std::time(NULL);
    ctx.pause_write_until_body_ready = true;

    Result<void> w = session.setBodyWatchFd_(stdout_fd);
    if (w.isError())
        return w;

    session.changeState(new SendResponseState());
    (void)session.updateSocketWatches_();

    // すでに body を先読みしている場合は、ここで送信を開始してよい。
    if (!ctx.waiting_cgi_first_body)
    {
        BodySource* bs = new PrefetchedFdBodySource(stdout_fd, prefetched);
        session.installBodySourceAndWriter_(utils::OwnedPtr<BodySource>(bs));
        ctx.cgi_stdout_fd_for_response = -1;
        ctx.cgi_prefetched_body.clear();
        ctx.pause_write_until_body_ready = false;

        // ヘッダ＋先頭chunkを send_buffer に積んで write watch を有効化
        if (ctx.response_writer != NULL)
            (void)ctx.response_writer->pump(ctx.send_buffer);
        (void)session.updateSocketWatches_();
    }

    return Result<void>();
}

Result<void> SessionCgiHandler::handleCgiHeadersReadyLocalRedirect_(
    HttpSession& session, CgiSession& cgi, const http::CgiResponse& cr)
{
    SessionContext& ctx = session.getContext();
    if (ctx.redirect_count >= HttpSession::kMaxInternalRedirects)
    {
        const int out_fd = cgi.releaseStdoutFd();
        if (out_fd >= 0)
            ::close(out_fd);

        if (ctx.active_cgi_session == &cgi)
        {
            controller_.requestDelete(ctx.active_cgi_session);
            ctx.active_cgi_session = NULL;
        }

        RequestProcessor::Output out;
        Result<void> bo =
            session.buildErrorOutput_(http::HttpStatus::SERVER_ERROR, &out);
        if (bo.isError())
            return bo;
        ctx.response.setHttpVersion(ctx.request.getHttpVersion());

        session.installBodySourceAndWriter_(out.body_source);

        ctx.should_close_connection =
            ctx.should_close_connection || ctx.peer_closed ||
            out.should_close_connection || !ctx.request.shouldKeepAlive() ||
            ctx.request_handler.shouldCloseConnection();
        session.changeState(new SendResponseState());
        (void)session.updateSocketWatches_();
        return Result<void>();
    }

    Result<std::string> loc = cr.getLocalRedirectUriPath();
    if (loc.isError())
        return Result<void>(ERROR, loc.getErrorMessage());

    const int out_fd = cgi.releaseStdoutFd();
    if (out_fd >= 0)
        ::close(out_fd);

    if (ctx.active_cgi_session == &cgi)
    {
        controller_.requestDelete(ctx.active_cgi_session);
        ctx.active_cgi_session = NULL;
    }

    ctx.redirect_count++;

    Result<http::HttpRequest> rr =
        session.buildInternalRedirectRequest_(loc.unwrap());
    if (rr.isError())
        return Result<void>(ERROR, rr.getErrorMessage());

    ctx.response.reset();
    ctx.request_handler.reset();
    ctx.request = rr.unwrap();

    return session.prepareResponseOrCgi_();
}

}  // namespace server
