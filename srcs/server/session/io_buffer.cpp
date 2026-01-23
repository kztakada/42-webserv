#include "server/session/io_buffer.hpp"

#include <unistd.h>

#include <algorithm>

namespace server
{

IoBuffer::IoBuffer() : storage_(), read_pos_(0), write_pos_(0) {}

IoBuffer::~IoBuffer() {}

const char* IoBuffer::data() const
{
    static const char kEmpty[1] = {0};
    if (size() == 0)
        return kEmpty;
    return reinterpret_cast<const char*>(&storage_[read_pos_]);
}

void IoBuffer::consume(size_t n)
{
    if (n == 0)
        return;
    if (n >= size())
    {
        storage_.clear();
        read_pos_ = 0;
        write_pos_ = 0;
        return;
    }
    read_pos_ += n;

    // 軽いコンパクション（先頭側が大きく空いたら詰める）
    if (read_pos_ > 4096)
    {
        const size_t remaining = size();
        std::copy(storage_.begin() + static_cast<std::ptrdiff_t>(read_pos_),
            storage_.begin() + static_cast<std::ptrdiff_t>(write_pos_),
            storage_.begin());
        storage_.resize(remaining);
        read_pos_ = 0;
        write_pos_ = remaining;
    }
}

void IoBuffer::append(const char* data, size_t n)
{
    if (data == NULL || n == 0)
        return;

    if (storage_.size() < write_pos_ + n)
        storage_.resize(write_pos_ + n);

    std::copy(data, data + n,
        storage_.begin() + static_cast<std::ptrdiff_t>(write_pos_));
    write_pos_ += n;
}

void IoBuffer::append(const std::string& s) { append(s.data(), s.size()); }

ssize_t IoBuffer::fillFromFd(int fd)
{
    // 読み込み量は適当なチャンク
    char buf[4096];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n > 0)
        append(buf, static_cast<size_t>(n));
    return n;
}

ssize_t IoBuffer::flushToFd(int fd)
{
    if (size() == 0)
        return 0;
    ssize_t n = ::write(fd, data(), size());
    if (n > 0)
        consume(static_cast<size_t>(n));
    return n;
}

}  // namespace server
