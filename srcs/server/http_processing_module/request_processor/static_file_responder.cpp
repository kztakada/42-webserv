#include "server/http_processing_module/request_processor/static_file_responder.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "http/content_types.hpp"
#include "server/session/fd_session/http_session/body_source.hpp"

namespace server
{

using utils::result::Result;

std::string StaticFileResponder::extractExtension_(const std::string& path)
{
    const std::string::size_type slash = path.find_last_of('/');
    const std::string::size_type dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return std::string();
    if (slash != std::string::npos && dot < slash)
        return std::string();
    return path.substr(dot);
}

Result<RequestProcessorOutput> StaticFileResponder::respondFile(
    const std::string& path, const struct stat& st,
    const http::HttpStatus& status, http::HttpResponse& out_response) const
{
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        return Result<RequestProcessorOutput>(ERROR, "open failed");
    }

    RequestProcessorOutput out;
    const unsigned long size = static_cast<unsigned long>(st.st_size);
    Result<void> s = out_response.setStatus(status);
    if (s.isError())
    {
        (void)::close(fd);
        return Result<RequestProcessorOutput>(ERROR, s.getErrorMessage());
    }

    (void)out_response.setExpectedContentLength(size);

    const std::string ext = extractExtension_(path);
    http::ContentType ct = http::ContentType::fromExtension(ext);
    (void)out_response.setHeader("Content-Type", ct.c_str());

    out.body_source.reset(new FileBodySource(fd, size));
    out.should_close_connection = false;
    return out;
}

}  // namespace server
