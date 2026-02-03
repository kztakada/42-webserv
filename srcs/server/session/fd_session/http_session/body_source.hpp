#ifndef WEBSERV_BODY_SOURCE_HPP_
#define WEBSERV_BODY_SOURCE_HPP_

#include <vector>

#include "utils/byte.hpp"
#include "utils/result.hpp"

namespace server
{

using namespace utils::result;

class BodySource
{
   public:
    enum ReadStatus
    {
        READ_OK,
        READ_EOF,
        READ_WOULD_BLOCK
    };

    struct ReadResult
    {
        ReadStatus status;
        std::vector<utils::Byte> data;

        ReadResult() : status(READ_OK), data() {}
    };

    virtual ~BodySource();

    // max_bytes まで読み出す。
    // READ_WOULD_BLOCK の場合は data は空。
    virtual Result<ReadResult> read(size_t max_bytes) = 0;
};

class FileBodySource : public BodySource
{
   public:
    // remaining_bytes == 0 の場合は「無制限」扱い（EOF まで読む）
    FileBodySource(int fd, unsigned long remaining_bytes);
    virtual ~FileBodySource();

    virtual Result<ReadResult> read(size_t max_bytes);

   private:
    int fd_;
    unsigned long remaining_bytes_;

    FileBodySource();
    FileBodySource(const FileBodySource& rhs);
    FileBodySource& operator=(const FileBodySource& rhs);
};

class CgiBodySource : public BodySource
{
   public:
    explicit CgiBodySource(int fd);
    virtual ~CgiBodySource();

    virtual Result<ReadResult> read(size_t max_bytes);

   private:
    int fd_;

    CgiBodySource();
    CgiBodySource(const CgiBodySource& rhs);
    CgiBodySource& operator=(const CgiBodySource& rhs);
};

}  // namespace server

#endif
