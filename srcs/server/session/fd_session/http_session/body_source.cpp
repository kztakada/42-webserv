#include "server/session/fd_session/http_session/body_source.hpp"

#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace server
{

BodySource::~BodySource() {}

FileBodySource::FileBodySource(int fd, unsigned long remaining_bytes)
    : fd_(fd), remaining_bytes_(remaining_bytes)
{
}

FileBodySource::~FileBodySource() {}

Result<BodySource::ReadResult> FileBodySource::read(size_t max_bytes)
{
    ReadResult r;

    if (max_bytes == 0)
    {
        r.status = READ_OK;
        return r;
    }

    size_t cap = max_bytes;
    if (remaining_bytes_ != 0 && cap > static_cast<size_t>(remaining_bytes_))
        cap = static_cast<size_t>(remaining_bytes_);

    if (remaining_bytes_ == 0)
    {
        // 無制限: cap はそのまま
    }
    else if (cap == 0)
    {
        r.status = READ_EOF;
        return r;
    }

    r.data.resize(cap);
    const ssize_t n = ::read(fd_, &r.data[0], cap);

    if (n < 0)
    {
        r.data.clear();
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            r.status = READ_WOULD_BLOCK;
            return r;
        }
        return Result<ReadResult>(ERROR, std::strerror(errno));
    }

    if (n == 0)
    {
        r.data.clear();
        r.status = READ_EOF;
        return r;
    }

    r.data.resize(static_cast<size_t>(n));
    r.status = READ_OK;

    if (remaining_bytes_ != 0)
    {
        remaining_bytes_ -= static_cast<unsigned long>(n);
        if (remaining_bytes_ == 0)
            r.status = READ_EOF;
    }

    return r;
}

CgiBodySource::CgiBodySource(int fd) : fd_(fd) {}

CgiBodySource::~CgiBodySource() {}

Result<BodySource::ReadResult> CgiBodySource::read(size_t max_bytes)
{
    ReadResult r;

    if (max_bytes == 0)
    {
        r.status = READ_OK;
        return r;
    }

    r.data.resize(max_bytes);
    const ssize_t n = ::read(fd_, &r.data[0], max_bytes);

    if (n < 0)
    {
        r.data.clear();
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            r.status = READ_WOULD_BLOCK;
            return r;
        }
        return Result<ReadResult>(ERROR, std::strerror(errno));
    }

    if (n == 0)
    {
        r.data.clear();
        r.status = READ_EOF;
        return r;
    }

    r.data.resize(static_cast<size_t>(n));
    r.status = READ_OK;
    return r;
}

}  // namespace server
