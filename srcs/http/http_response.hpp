#ifndef HTTP_HTTP_RESPONSE_HPP_
#define HTTP_HTTP_RESPONSE_HPP_

#include <string>
#include <vector>

#include "http/header.hpp"
#include "http/status.hpp"
#include "utils/result.hpp"

namespace http
{

using namespace utils::result;

// HTTPレスポンス（メタデータ + 状態）
// - ボディは保持しない（ストリーミング前提）
// - 送出バイト列の生成は HttpResponseEncoder に委譲する
class HttpResponse
{
   public:
    enum Phase
    {
        kWaitingForHeaders,
        kHeadersFlushed,
        kComplete,
        kError
    };

    HttpResponse();
    explicit HttpResponse(HttpStatus status);
    HttpResponse(const HttpResponse& rhs);
    HttpResponse& operator=(const HttpResponse& rhs);
    ~HttpResponse();

    void reset();

    Phase phase() const;
    bool isComplete() const;
    bool hasError() const;

    // ステータス設定
    Result<void> setStatus(HttpStatus status);
    Result<void> setStatus(HttpStatus status, const std::string& reason_phrase);
    HttpStatus getStatus() const;
    const std::string& getReasonPhrase() const;

    // HTTPバージョン（原則: RequestProcessor が Request に合わせて設定）
    Result<void> setHttpVersion(const std::string& version);
    const std::string& getHttpVersion() const;

    // ヘッダー操作
    Result<void> setHeader(const std::string& name, const std::string& value);
    Result<void> appendHeader(
        const std::string& name, const std::string& value);
    Result<void> removeHeader(const std::string& name);
    bool hasHeader(const std::string& name) const;
    Result<const std::vector<std::string>&> getHeader(
        const std::string& name) const;
    const HeaderMap& getHeaders() const;

    // Content-Length（あれば保持）
    bool hasExpectedContentLength() const;
    unsigned long expectedContentLength() const;
    Result<void> setExpectedContentLength(unsigned long n);

    // ResponseWriter/Encoder から呼ばれる状態遷移
    Result<void> markHeadersFlushed();
    Result<void> markComplete();

   private:
    static const std::string kDefaultHttpVersion;

    Result<void> ensureHeaderEditable_() const;
    Result<void> refreshExpectedContentLength_();

    Phase phase_;
    std::string http_version_;
    HttpStatus status_;
    std::string reason_phrase_;
    HeaderMap headers_;

    bool has_expected_content_length_;
    unsigned long expected_content_length_;
};

}  // namespace http

#endif