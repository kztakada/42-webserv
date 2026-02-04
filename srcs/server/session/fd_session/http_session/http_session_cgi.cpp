#include <map>
#include <sstream>

#include "http/cgi_meta_variables.hpp"
#include "server/session/fd/cgi_pipe/cgi_pipe_fd.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"

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

Result<void> HttpSession::startCgi_()
{
    if (!handler_.hasLocationRouting())
        return Result<void>(ERROR, "missing routing for CGI");

    Result<server::CgiContext> ctxr =
        handler_.getLocationRouting().getCgiContext();
    if (ctxr.isError())
        return Result<void>(ERROR, ctxr.getErrorMessage());
    const server::CgiContext ctx = ctxr.unwrap();

    // Request body fd
    int request_body_fd = -1;
    if (request_.hasBody())
    {
        (void)handler_.bodyStore().finish();
        Result<int> fd = handler_.bodyStore().openForRead();
        if (fd.isOk())
            request_body_fd = fd.unwrap();
    }

    // CGI env
    http::CgiMetaVariables meta = http::CgiMetaVariables::fromHttpRequest(
        request_, ctx.script_name, ctx.path_info);

    // 追加のメタ変数（最小限）
    meta.setServerName(socket_fd_.getServerIp().toString());
    {
        std::istringstream iss(socket_fd_.getServerPort().toString());
        unsigned int p = 0;
        iss >> p;
        meta.setServerPort(p);
    }
    meta.setRemoteAddr(socket_fd_.getClientIp().toString());
    meta.setServerSoftware("webserv");

    std::map<std::string, std::string> env = meta.getAll();
    env["SCRIPT_FILENAME"] = ctx.script_filename.str();
    env["QUERY_STRING"] = ctx.query_string;
    if (isPhpCgiExecutor_(ctx.executor_path.str()))
        env["REDIRECT_STATUS"] = "200";

    // 実行: executor を execve し、script_filename を引数に渡す
    std::vector<std::string> args;
    args.push_back(ctx.script_filename.str());

    const std::string working_dir = dirnameOf_(ctx.script_filename.str());
    Result<CgiSpawnResult> spawned = server::CgiPipeFd::Execute(
        ctx.executor_path.str(), args, env, working_dir);
    if (spawned.isError())
        return Result<void>(ERROR, spawned.getErrorMessage());

    const CgiSpawnResult s = spawned.unwrap();
    active_cgi_session_ = new CgiSession(s.pid, s.stdin_fd, s.stdout_fd,
        s.stderr_fd, request_body_fd, this, controller_);

    Result<void> d = controller_.delegateSession(active_cgi_session_);
    if (d.isError())
    {
        delete active_cgi_session_;
        active_cgi_session_ = NULL;
        return Result<void>(ERROR, d.getErrorMessage());
    }

    state_ = EXECUTE_CGI;
    (void)updateSocketWatches_();
    return Result<void>();
}

}  // namespace server
