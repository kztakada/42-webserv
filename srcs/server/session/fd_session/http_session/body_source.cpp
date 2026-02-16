#include "server/session/fd_session/http_session/body_source.hpp"

#include <unistd.h>

#include <cstddef>

namespace server
{

BodySource::~BodySource() {}

FileBodySource::FileBodySource(int fd, unsigned long remaining_bytes)
    : fd_(fd), remaining_bytes_(remaining_bytes)
{
}

FileBodySource::~FileBodySource()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

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
        return Result<ReadResult>(ERROR, "FileBodySource read failed");
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

CgiBodySource::~CgiBodySource()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

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
        return Result<ReadResult>(ERROR, "CgiBodySource read failed");
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

PrefetchedFdBodySource::PrefetchedFdBodySource(
    int fd, const std::vector<utils::Byte>& prefetched)
    : fd_(fd), prefetched_(prefetched), prefetched_pos_(0)
{
}

PrefetchedFdBodySource::~PrefetchedFdBodySource()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

Result<BodySource::ReadResult> PrefetchedFdBodySource::read(size_t max_bytes)
{
    ReadResult r;

    if (max_bytes == 0)
    {
        r.status = READ_OK;
        return r;
    }

    // 先読み分が残っていれば優先
    if (prefetched_pos_ < prefetched_.size())
    {
        size_t remain = prefetched_.size() - prefetched_pos_;
        size_t n = max_bytes;
        if (n > remain)
            n = remain;

        r.data.insert(r.data.end(),
            prefetched_.begin() + static_cast<std::ptrdiff_t>(prefetched_pos_),
            prefetched_.begin() +
                static_cast<std::ptrdiff_t>(prefetched_pos_ + n));
        prefetched_pos_ += n;

        r.status = READ_OK;
        return r;
    }

    // 以降は fd から読む
    r.data.resize(max_bytes);
    const ssize_t n = ::read(fd_, &r.data[0], max_bytes);

    if (n < 0)
    {
        return Result<ReadResult>(ERROR, "PrefetchedFdBodySource read failed");
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

StringBodySource::StringBodySource(const std::string& body) : body_(), pos_(0)
{
    body_.reserve(body.size());
    for (std::string::size_type i = 0; i < body.size(); ++i)
    {
        body_.push_back(static_cast<utils::Byte>(body[i]));
    }
}

StringBodySource::~StringBodySource() {}

Result<BodySource::ReadResult> StringBodySource::read(size_t max_bytes)
{
    ReadResult r;

    if (pos_ >= body_.size())
    {
        r.status = READ_EOF;
        return r;
    }

    if (max_bytes == 0)
    {
        r.status = READ_OK;
        return r;
    }

    size_t remain = body_.size() - pos_;
    size_t n = max_bytes;
    if (n > remain)
        n = remain;

    r.data.insert(r.data.end(),
        body_.begin() + static_cast<std::ptrdiff_t>(pos_),
        body_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
    pos_ += n;

    if (pos_ >= body_.size())
        r.status = READ_EOF;
    else
        r.status = READ_OK;

    return r;
}

}  // namespace server
