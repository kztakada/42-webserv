#include "cgi_pipe_fd.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>

namespace server
{

CgiPipeFd::CgiPipeFd(int fd) : FdBase(fd) {}

CgiPipeFd::~CgiPipeFd()
{
    if (fd_ >= 0)
        close(fd_);
}

// void CgiPipeFd::KillProcess() {
//   if (cgi_pid_ > 0) {
//     kill(cgi_pid_, SIGKILL);
//   }
// }

// bool CgiPipeFd::IsProcessAlive() const {
//   if (cgi_pid_ <= 0) {
//     return false;
//   }
//   int status;
//   pid_t result = waitpid(cgi_pid_, &status, WNOHANG);
//   return result == 0;
// }

// int CgiPipeFd::WaitProcess() {
//   if (cgi_pid_ <= 0) {
//     return -1;
//   }
//   int status;
//   waitpid(cgi_pid_, &status, 0);
//   return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
// }

std::string CgiPipeFd::GetResourceType() const { return "CgiPipeFd"; }

Result<void> CgiPipeFd::Execute(const std::string& script_path,
    const std::vector<std::string>& args,
    const std::map<std::string, std::string>& env_vars,
    const std::string& working_dir)
{
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0)
    {
        return Result<void>(ERROR, "pipe() failed");
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return Result<void>(ERROR, "fork() failed");
    }

    if (pid == 0)
    {
        // 子プロセス
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        if (!working_dir.empty())
        {
            chdir(working_dir.c_str());
        }

        SetupChildEnvironment(env_vars);

        // 引数の準備
        std::vector<char*> argv_ptrs;
        argv_ptrs.push_back(const_cast<char*>(script_path.c_str()));
        for (size_t i = 0; i < args.size(); ++i)
        {
            argv_ptrs.push_back(const_cast<char*>(args[i].c_str()));
        }
        argv_ptrs.push_back(NULL);

        execv(script_path.c_str(), &argv_ptrs[0]);
        exit(1);
    }

    // 親プロセス
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    CgiPipeFd* pipe_stdin =
        new CgiPipeFd(stdin_pipe[1]);  // 書き込み後にdeleteすればcloseされる
    CgiPipeFd* pipe_stdout = new CgiPipeFd(stdout_pipe[0]);
    CgiPipeFd* pipe_stderr = new CgiPipeFd(stderr_pipe[0]);

    return Result<void>(OK);
}

void CgiPipeFd::SetupChildEnvironment(
    const std::map<std::string, std::string>& env_vars)
{
    for (std::map<std::string, std::string>::const_iterator it =
             env_vars.begin();
        it != env_vars.end(); ++it)
    {
        setenv(it->first.c_str(), it->second.c_str(), 1);
    }
}

void CgiPipeFd::CloseUnusedFds(int max_fd)
{
    for (int fd = 3; fd < max_fd; ++fd)
    {
        close(fd);
    }
}

}  // namespace server
