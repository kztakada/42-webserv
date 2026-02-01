#ifndef WEBSERV_CGI_PIPE_FD_HPP_
#define WEBSERV_CGI_PIPE_FD_HPP_

#include <sys/types.h>

#include <map>
#include <string>
#include <vector>

#include "server/session/fd/fd_base.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;

struct CgiSpawnResult
{
    pid_t pid;
    int stdin_fd;   // parent writes -> child stdin
    int stdout_fd;  // parent reads  <- child stdout
    int stderr_fd;  // parent reads  <- child stderr
};

// CGIプロセスとのパイプ通信を管理（fork + pipe 実装）
class CgiPipeFd : public FdBase
{
   public:
    CgiPipeFd(int fd);
    virtual ~CgiPipeFd();

    bool isSame(int fd) const { return fd_ == fd; }

    virtual std::string GetResourceType() const;

    // 静的ファクトリメソッド
    static Result<CgiSpawnResult> Execute(const std::string& script_path,
        const std::vector<std::string>& args,
        const std::map<std::string, std::string>& env_vars,
        const std::string& working_dir);

   private:
    CgiPipeFd();
    CgiPipeFd(const CgiPipeFd& rhs);
    CgiPipeFd& operator=(const CgiPipeFd& rhs);

    static std::vector<std::string> buildEnvEntries(
        const std::map<std::string, std::string>& env_vars);
};

}  // namespace server

#endif
