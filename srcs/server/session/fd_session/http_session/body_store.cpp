#include "server/session/fd_session/http_session/body_store.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <sstream>

namespace server
{

using namespace utils::result;

static Result<void> writeAll(int fd, const utils::Byte* data, size_t len)
{
    if (len == 0)
        return Result<void>();
    if (data == NULL)
        return Result<void>(ERROR, "null body data");

    size_t off = 0;
    while (off < len)
    {
        const size_t chunk = len - off;
        const ssize_t w = ::write(fd, data + off, chunk);
        if (w < 0)
            return Result<void>(ERROR, "write() failed");
        if (w == 0)
            return Result<void>(ERROR, "write() returned 0");
        off += static_cast<size_t>(w);
    }
    return Result<void>();
}

std::string BodyStore::buildPath_(const void* unique_key)
{
    std::ostringstream oss;
    oss << "/tmp/webserv_body_" << reinterpret_cast<unsigned long>(unique_key)
        << ".tmp";
    return oss.str();
}

BodyStore::BodyStore(const void* unique_key)
    : path_(buildPath_(unique_key)), write_fd_(-1), size_bytes_(0)
{
}

BodyStore::~BodyStore() { reset(); }

void BodyStore::reset()
{
    if (write_fd_ >= 0)
    {
        ::close(write_fd_);
        write_fd_ = -1;
    }
    size_bytes_ = 0;

    // 使い回し方針でも「残骸を残さない」ために削除を試みる。
    // 失敗しても致命ではないので、呼び出し側が必要なら結果を確認する。
    (void)removeFile();
}

Result<void> BodyStore::removeFile()
{
    if (path_.empty())
        return Result<void>();

    // std::remove は失敗時に errno を立てるが、ここでは詳細は扱わない。
    // 未作成・既に削除済みのケースもあるため、失敗は致命としない運用を想定。
    const int rc = std::remove(path_.c_str());
    if (rc == 0)
        return Result<void>();
    return Result<void>(ERROR, "remove() failed");
}

Result<void> BodyStore::begin()
{
    if (write_fd_ >= 0)
        return Result<void>();

    const int fd = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return Result<void>(ERROR, "open() failed");

    write_fd_ = fd;
    size_bytes_ = 0;
    return Result<void>();
}

Result<void> BodyStore::append(const utils::Byte* data, size_t len)
{
    Result<void> r = begin();
    if (r.isError())
        return r;

    r = writeAll(write_fd_, data, len);
    if (r.isError())
        return r;

    size_bytes_ += len;
    return Result<void>();
}

Result<void> BodyStore::finish()
{
    if (write_fd_ < 0)
        return Result<void>();

    ::close(write_fd_);
    write_fd_ = -1;
    return Result<void>();
}

Result<int> BodyStore::openForRead() const
{
    const int fd = ::open(path_.c_str(), O_RDONLY);
    if (fd < 0)
        return Result<int>(ERROR, "open() failed");
    return fd;
}

}  // namespace server
