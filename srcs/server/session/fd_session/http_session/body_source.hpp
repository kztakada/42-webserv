#ifndef WEBSERV_BODY_SOURCE_HPP_
#define WEBSERV_BODY_SOURCE_HPP_

#include <string>
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

// 先読みデータ（prefetched）を優先して返し、その後に fd から読み続ける。
// CGI stdout のヘッダ終端検出で発生する「余剰 body bytes」を失わずに
// HttpResponseWriter へ渡す用途を想定。
class PrefetchedFdBodySource : public BodySource
{
   public:
    PrefetchedFdBodySource(int fd, const std::vector<utils::Byte>& prefetched);
    virtual ~PrefetchedFdBodySource();

    virtual Result<ReadResult> read(size_t max_bytes);

   private:
    int fd_;
    std::vector<utils::Byte> prefetched_;
    size_t prefetched_pos_;

    PrefetchedFdBodySource();
    PrefetchedFdBodySource(const PrefetchedFdBodySource& rhs);
    PrefetchedFdBodySource& operator=(const PrefetchedFdBodySource& rhs);
};

// 文字列（メモリ）から body を供給する。
// error page など、ファイルを用意せずに返したい小さめのレスポンスで使用。
class StringBodySource : public BodySource
{
   public:
    explicit StringBodySource(const std::string& body);
    virtual ~StringBodySource();

    virtual Result<ReadResult> read(size_t max_bytes);

   private:
    std::vector<utils::Byte> body_;
    size_t pos_;

    StringBodySource();
    StringBodySource(const StringBodySource& rhs);
    StringBodySource& operator=(const StringBodySource& rhs);
};

}  // namespace server

#endif
