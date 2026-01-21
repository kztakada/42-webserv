#ifndef HTTP_HTTP_REQUEST_HPP_
#define HTTP_HTTP_REQUEST_HPP_

#include <cstddef>
#include <string>
#include <vector>

#include "http/header.hpp"
#include "http/http_method.hpp"
#include "http/status.hpp"
#include "utils/byte.hpp"
#include "utils/result.hpp"

namespace http
{

using namespace utils::result;

// RFC 7230準拠のHTTPリクエストパーサー
class HttpRequest
{
   public:
    HttpRequest();
    HttpRequest(const HttpRequest& rhs);
    HttpRequest& operator=(const HttpRequest& rhs);
    ~HttpRequest();

    // パース機能（RFC 7230準拠）
    Result<size_t> parse(const std::vector<utils::Byte>& buffer);

    bool isParseComplete() const;
    bool hasParseError() const;
    HttpStatus getParseErrorStatus() const;

    // Getter (RFC準拠の情報のみ)
    HttpMethod getMethod() const;
    const std::string& getMethodString() const;
    const std::string& getPath() const;
    const std::string& getQueryString() const;
    std::string getHttpVersion() const;
    int getMinorVersion() const;

    const HeaderMap& getHeaders() const;
    Result<const std::vector<std::string>&> getHeader(
        const std::string& name) const;
    bool hasHeader(const std::string& name) const;

    const std::vector<utils::Byte>& getBody() const;
    unsigned long getBodySize() const;
    bool isChunkedEncoding() const;

   private:
    enum ParsingPhase
    {
        kRequestLine,
        kHeaderField,
        kBody,
        kComplete,
        kError
    };

    HttpMethod method_;
    std::string method_string_;
    std::string path_;
    std::string query_string_;
    int minor_version_;
    HeaderMap headers_;
    std::vector<utils::Byte> body_;

    ParsingPhase phase_;
    HttpStatus parse_error_status_;
    bool is_chunked_;
    unsigned long content_length_;
    size_t cursor_;

    // パース内部関数
    Result<void> parseRequestLine(const std::string& line);
    Result<void> parseHeaderLine(const std::string& line);
    Result<void> parseMethod(const std::string& method);
    Result<void> parsePath(const std::string& path);
    Result<void> parseVersion(const std::string& version);
    Result<void> validateHeaders();
    std::string extractLine(const std::vector<utils::Byte>& buffer);
    static std::string trimOws(const std::string& s);
};

}  // namespace http

#endif