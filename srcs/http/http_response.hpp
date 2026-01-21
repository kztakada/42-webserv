#ifndef HTTP_HTTP_RESPONSE_HPP_
#define HTTP_HTTP_RESPONSE_HPP_

#include <string>
#include <vector>

#include "http/header.hpp"
#include "http/status.hpp"
#include "utils/byte.hpp"
#include "utils/result.hpp"

namespace http
{

using namespace utils::result;

// RFC 7230準拠のHTTPレスポンスビルダー
class HttpResponse
{
   public:
    HttpResponse();
    explicit HttpResponse(HttpStatus status);
    HttpResponse(const HttpResponse& rhs);
    HttpResponse& operator=(const HttpResponse& rhs);
    ~HttpResponse();

    // ステータス設定
    void setStatus(HttpStatus status);
    void setStatus(HttpStatus status, const std::string& reason_phrase);
    HttpStatus getStatus() const;

    // HTTPバージョン設定
    void setHttpVersion(const std::string& version);
    const std::string& getHttpVersion() const;

    // ヘッダー操作（RFC 7230準拠）
    void setHeader(const std::string& name, const std::string& value);
    void appendHeader(const std::string& name, const std::string& value);
    void removeHeader(const std::string& name);
    bool hasHeader(const std::string& name) const;
    Result<const std::vector<std::string>&> getHeader(
        const std::string& name) const;
    const HeaderMap& getHeaders() const;

    // ボディ設定
    void setBody(const std::vector<utils::Byte>& body);
    void setBody(const std::string& body);
    void appendBody(const std::vector<utils::Byte>& data);
    void appendBody(const std::string& data);
    const std::vector<utils::Byte>& getBody() const;

    // ファイルディスクリプタからのボディ設定（効率的なファイル送信用）
    void setBodyFromFile(int fd, unsigned long size);
    bool hasFileBody() const;
    int getBodyFileDescriptor() const;
    unsigned long getBodyFileSize() const;

    // シリアライゼーション（バイト列への変換）
    std::vector<utils::Byte> serialize() const;
    std::vector<utils::Byte> serializeStatusLine() const;
    std::vector<utils::Byte> serializeHeaders() const;

    // チャンク転送エンコーディング（RFC 7230 Section 4.1）
    static std::vector<utils::Byte> createChunk(
        const std::vector<utils::Byte>& data);
    static std::vector<utils::Byte> createLastChunk();

   private:
    static const std::string kDefaultHttpVersion;

    std::string http_version_;
    HttpStatus status_;
    std::string reason_phrase_;
    HeaderMap headers_;
    std::vector<utils::Byte> body_;

    // ファイルボディ用（オプション）
    int body_file_fd_;
    unsigned long body_file_size_;

    void updateContentLength();
};

}  // namespace http

#endif