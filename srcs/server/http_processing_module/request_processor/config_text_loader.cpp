#include "server/http_processing_module/request_processor/config_text_loader.hpp"

#include <fcntl.h>
#include <unistd.h>

namespace server
{

using utils::result::ERROR;
using utils::result::Result;

Result<std::string> ConfigTextLoader::readFileToString_(const std::string& path)
{
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        return Result<std::string>(ERROR, std::string(), "open failed");
    }

    std::string out;
    char buf[4096];
    for (;;)
    {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n == 0)
            break;
        if (n < 0)
        {
            (void)::close(fd);
            return Result<std::string>(ERROR, std::string(), "read failed");
        }
        out.append(buf, buf + n);
    }
    (void)::close(fd);
    if (out.empty())
    {
        return Result<std::string>(ERROR, std::string(), "file is empty");
    }
    return out;
}

Result<std::string> ConfigTextLoader::load(const std::string& file_name)
{
    const std::string p1 = std::string("server/config/") + file_name;
    Result<std::string> r1 = readFileToString_(p1);
    if (r1.isOk())
        return r1.unwrap();

    const std::string p2 = std::string("srcs/server/config/") + file_name;
    Result<std::string> r2 = readFileToString_(p2);
    if (r2.isOk())
        return r2.unwrap();

    return Result<std::string>(ERROR, std::string(), "config file not found");
}

}  // namespace server
