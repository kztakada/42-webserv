#include "server/session/fd/cgi_pipe/cgi_pipe_fd.hpp"

#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
using utils::result::Result;

class UniqueFd
{
   public:
    explicit UniqueFd(int fd) : fd_(fd) {}
    ~UniqueFd()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int get() const { return fd_; }

    void closeNow()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

   private:
    int fd_;

    UniqueFd();
    UniqueFd(const UniqueFd&);
    UniqueFd& operator=(const UniqueFd&);
};

std::string readAll_(int fd)
{
    std::string out;
    char buf[512];
    while (true)
    {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0)
        {
            out.append(buf, buf + n);
            continue;
        }
        if (n == 0)
        {
            break;
        }
        if (errno == EINTR)
        {
            continue;
        }
        // unexpected read error
        break;
    }
    return out;
}

}  // namespace

TEST(CgiPipeFd, ExecutesChildAndPipesStdIO)
{
    std::map<std::string, std::string> env;
    env["PATH"] = "/usr/bin:/bin";

    std::vector<std::string> args;
    args.push_back("-c");
    args.push_back("cat");

    Result<server::CgiSpawnResult> r =
        server::CgiPipeFd::Execute("/bin/sh", args, env, "");
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    server::CgiSpawnResult spawned = r.unwrap();
    ASSERT_GT(spawned.pid, 0);

    UniqueFd in_fd(spawned.stdin_fd);
    UniqueFd out_fd(spawned.stdout_fd);
    UniqueFd err_fd(spawned.stderr_fd);

    const char* msg = "hello\n";
    ssize_t w = ::write(in_fd.get(), msg, std::strlen(msg));
    ASSERT_EQ(w, static_cast<ssize_t>(std::strlen(msg)));
    in_fd.closeNow();

    std::string out = readAll_(out_fd.get());
    std::string err = readAll_(err_fd.get());

    EXPECT_EQ(out, std::string("hello\n"));
    EXPECT_TRUE(err.empty());

    int status = 0;
    pid_t waited = ::waitpid(spawned.pid, &status, 0);
    ASSERT_EQ(waited, spawned.pid);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(CgiPipeFd, PassesEnvironmentVariablesToChild)
{
    std::map<std::string, std::string> env;
    env["FOO"] = "BAR";
    env["PATH"] = "/usr/bin:/bin";

    std::vector<std::string> args;
    args.push_back("-c");
    args.push_back("printf %s \"$FOO\"");

    Result<server::CgiSpawnResult> r =
        server::CgiPipeFd::Execute("/bin/sh", args, env, "");
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    server::CgiSpawnResult spawned = r.unwrap();
    ASSERT_GT(spawned.pid, 0);

    UniqueFd in_fd(spawned.stdin_fd);
    UniqueFd out_fd(spawned.stdout_fd);
    UniqueFd err_fd(spawned.stderr_fd);

    in_fd.closeNow();

    std::string out = readAll_(out_fd.get());
    std::string err = readAll_(err_fd.get());

    EXPECT_EQ(out, std::string("BAR"));
    EXPECT_TRUE(err.empty());

    int status = 0;
    pid_t waited = ::waitpid(spawned.pid, &status, 0);
    ASSERT_EQ(waited, spawned.pid);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}
