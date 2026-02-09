#include "cgi_pipe_fd.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <string>

namespace server
{

CgiPipeFd::CgiPipeFd(int fd) : FdBase(fd) {}

CgiPipeFd::~CgiPipeFd() {}

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

namespace
{

Result<void> setNonBlocking_(int fd)
{
    if (fd < 0)
        return Result<void>(ERROR, "invalid fd");
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return Result<void>(ERROR, "fcntl(F_GETFL) failed");
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return Result<void>(ERROR, "fcntl(F_SETFL) failed");
    return Result<void>();
}

void closeIfValid_(int& fd)
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}

void closePipeIfValid_(int pipefd[2])
{
    closeIfValid_(pipefd[0]);
    closeIfValid_(pipefd[1]);
}

}  // namespace

std::vector<std::string> CgiPipeFd::buildEnvEntries(
    const std::map<std::string, std::string>& env_vars)
{
    std::vector<std::string> entries;
    for (std::map<std::string, std::string>::const_iterator it =
             env_vars.begin();
        it != env_vars.end(); ++it)
    {
        entries.push_back(it->first + "=" + it->second);
    }
    return entries;
}

Result<CgiSpawnResult> CgiPipeFd::Execute(const std::string& script_path,
    const std::vector<std::string>& args,
    const std::map<std::string, std::string>& env_vars,
    const std::string& working_dir)
{
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0)
    {
        closePipeIfValid_(stdin_pipe);
        closePipeIfValid_(stdout_pipe);
        closePipeIfValid_(stderr_pipe);
        return Result<CgiSpawnResult>(ERROR, "pipe() failed");
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        closePipeIfValid_(stdin_pipe);
        closePipeIfValid_(stdout_pipe);
        closePipeIfValid_(stderr_pipe);
        return Result<CgiSpawnResult>(ERROR, "fork() failed");
    }

    if (pid == 0)
    {
        // 子プロセス
        if (::dup2(stdin_pipe[0], STDIN_FILENO) < 0 ||
            ::dup2(stdout_pipe[1], STDOUT_FILENO) < 0 ||
            ::dup2(stderr_pipe[1], STDERR_FILENO) < 0)
        {
            ::_exit(1);
        }

        closePipeIfValid_(stdin_pipe);
        closePipeIfValid_(stdout_pipe);
        closePipeIfValid_(stderr_pipe);

        if (!working_dir.empty())
        {
            if (::chdir(working_dir.c_str()) < 0)
            {
                ::_exit(1);
            }
        }

        // 引数の準備
        std::vector<char*> argv_ptrs;
        argv_ptrs.push_back(const_cast<char*>(script_path.c_str()));
        for (size_t i = 0; i < args.size(); ++i)
        {
            argv_ptrs.push_back(const_cast<char*>(args[i].c_str()));
        }
        argv_ptrs.push_back(NULL);

        std::vector<std::string> env_entries = buildEnvEntries(env_vars);
        std::vector<char*> envp_ptrs;
        for (size_t i = 0; i < env_entries.size(); ++i)
        {
            envp_ptrs.push_back(const_cast<char*>(env_entries[i].c_str()));
        }
        envp_ptrs.push_back(NULL);

        ::execve(script_path.c_str(), &argv_ptrs[0], &envp_ptrs[0]);
        ::_exit(127);
    }

    // 親プロセス
    closeIfValid_(stdin_pipe[0]);
    closeIfValid_(stdout_pipe[1]);
    closeIfValid_(stderr_pipe[1]);

    // 親側の fd は non-blocking にする（Session 実装が would-block
    // を前提にしている）
    Result<void> nb;
    nb = setNonBlocking_(stdin_pipe[1]);
    if (nb.isError())
    {
        closeIfValid_(stdin_pipe[1]);
        closeIfValid_(stdout_pipe[0]);
        closeIfValid_(stderr_pipe[0]);
        return Result<CgiSpawnResult>(ERROR, nb.getErrorMessage());
    }
    nb = setNonBlocking_(stdout_pipe[0]);
    if (nb.isError())
    {
        closeIfValid_(stdin_pipe[1]);
        closeIfValid_(stdout_pipe[0]);
        closeIfValid_(stderr_pipe[0]);
        return Result<CgiSpawnResult>(ERROR, nb.getErrorMessage());
    }
    nb = setNonBlocking_(stderr_pipe[0]);
    if (nb.isError())
    {
        closeIfValid_(stdin_pipe[1]);
        closeIfValid_(stdout_pipe[0]);
        closeIfValid_(stderr_pipe[0]);
        return Result<CgiSpawnResult>(ERROR, nb.getErrorMessage());
    }

    CgiSpawnResult spawned;
    spawned.pid = pid;
    spawned.stdin_fd = stdin_pipe[1];
    spawned.stdout_fd = stdout_pipe[0];
    spawned.stderr_fd = stderr_pipe[0];
    return spawned;
}

}  // namespace server
