#include "server/session/fd_session/http_session/body_store.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <sstream>

namespace server
{

using namespace utils::result;

static std::string parentDir_(const std::string& path)
{
    const std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return ".";
    if (pos == 0)
        return "/";
    return path.substr(0, pos);
}

static bool canCreateFileInDir_(const std::string& dir)
{
    return ::access(dir.c_str(), W_OK | X_OK) == 0;
}

static bool canWriteExistingFile_(const std::string& path)
{
    return ::access(path.c_str(), W_OK) == 0;
}

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
    : default_path_(buildPath_(unique_key)),
      path_(default_path_),
      write_fd_(-1),
      size_bytes_(0),
      remove_on_reset_(true),
      allow_overwrite_(true),
      is_committed_(false)
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

    // upload_store の場合でも、正常完了（commit）していなければ削除する。
    if (remove_on_reset_ || !is_committed_)
    {
        // 使い回し方針でも「残骸を残さない」ために削除を試みる。
        // 失敗しても致命ではないので、呼び出し側が必要なら結果を確認する。
        (void)removeFile();
    }

    // 次のリクエストでは必ず一時ファイルに戻す。
    path_ = default_path_;
    remove_on_reset_ = true;
    allow_overwrite_ = true;
    is_committed_ = false;
}

Result<void> BodyStore::configureForUpload(
    const std::string& destination_path, bool allow_overwrite)
{
    if (write_fd_ >= 0)
        return Result<void>(ERROR, "body store is already opened");
    if (destination_path.empty())
        return Result<void>(ERROR, "destination path is empty");

    path_ = destination_path;
    remove_on_reset_ = false;
    allow_overwrite_ = allow_overwrite;
    return Result<void>();
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

    // 事前に access() で書き込み可否を判定する（失敗後の errno
    // 参照に頼らない）。 NOTE: TOCTTOU
    // はあるが、HTTPステータス判断の安定化を優先する。
    if (!path_.empty())
    {
        if (::access(path_.c_str(), F_OK) == 0)
        {
            if (allow_overwrite_)
            {
                if (!canWriteExistingFile_(path_))
                    return Result<void>(ERROR, "forbidden");
            }
            else
            {
                // allow_overwrite=false かつ既存ファイルありは open(O_EXCL)
                // で失敗想定。 ここでは権限ではないので forbidden にしない。
            }
        }
        else
        {
            const std::string dir = parentDir_(path_);
            if (!canCreateFileInDir_(dir))
                return Result<void>(ERROR, "forbidden");
        }
    }

    int flags = O_WRONLY | O_CREAT;
    if (allow_overwrite_)
        flags |= O_TRUNC;
    else
        flags |= O_EXCL;

    const int fd = ::open(path_.c_str(), flags, 0644);
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
