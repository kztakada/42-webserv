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

// RFC 9112準拠のHTTPリクエストパーサー
class HttpRequest
{
   public:
    // HTTPリクエストの最大量を定義してDos耐性を上げる
    struct Limits
    {
        // 0 の場合は無制限
        size_t max_request_line_length;  // CRLF を除いた長さ
        size_t max_header_bytes;         // CRLF を含むヘッダセクションの総量
        size_t max_header_count;         // ヘッダ行数（空行を除く）
        size_t max_body_bytes;           // デコード後のメッセージボディ総量

        Limits();
    };

    HttpRequest();
    HttpRequest(const HttpRequest& rhs);
    HttpRequest& operator=(const HttpRequest& rhs);
    ~HttpRequest();

    // パース機能（RFC 9112準拠）
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

    // 防御的制限（DoS耐性）
    void setLimits(const Limits& limits);
    const Limits& getLimits() const;

   private:
    static const size_t kDefaultMaxRequestLineLength;
    static const size_t kDefaultMaxHeaderBytes;
    static const size_t kDefaultMaxHeaderCount;
    static const size_t kDefaultMaxBodyBytes;

    // パースの状態ステータス
    enum ParsingPhase
    {
        kRequestLine,  // リクエストライン処理中
        kHeaderField,  // ヘッダーフィールド処理中
        kBody,         // ボディ処理中
        kComplete,     // パース処理完了
        kError         // パース処理エラー
    };

    // メッセージボディのフレーミング方式
    enum BodyFraming
    {
        kNoBody,         // ボディなし
        kContentLength,  // ContentLengthタイプ
        kChunked,        // チャンクタイプ
        kUntilClose  // コネクションが閉じるまでボディの終端を認識しない（今回は未実装）
    };

    // Chunkモードの時のボディの処理状況
    enum ChunkPhase
    {
        kChunkSizeLine,  // chunk-size処理中（5;key=value）
        kChunkData,      // chunk-data処理中（データの中身）
        kChunkDataCrlf,  // chunk-dataの直後にあるCRLFを処理中(\r\n)
        kChunkTrailer,   // trailer処理中（ヘッダフィールドの追加情報）
        kChunkDone       // チャンク式のボディ処理完了
    };

    // パース状況の管理
    ParsingPhase phase_;
    HttpStatus parse_error_status_;

    // 処理済み文字数
    size_t cursor_;

    // リクエストライン
    HttpMethod method_;
    std::string method_string_;
    std::string path_;
    std::string query_string_;
    int minor_version_;

    // ヘッダセクション
    HeaderMap headers_;

    // メッセージボディ
    BodyFraming body_framing_;
    std::vector<utils::Byte> body_;

    // content-lengthタイプの管理
    unsigned long content_length_remaining_;

    // chunkタイプの実デコード管理
    ChunkPhase chunk_phase_;
    unsigned long chunk_bytes_remaining_;

    // 防御的制限（デフォルト有効）
    Limits limits_;
    size_t header_bytes_parsed_;
    size_t header_count_;

    // パース内部関数
    Result<void> parseRequestLine(const std::string& line);
    Result<void> parseHeaderLine(const std::string& line);
    Result<void> parseMethod(const std::string& method);
    Result<void> parseRequestTarget(const std::string& target);
    Result<void> parseVersion(const std::string& version);
    Result<void> validateHeaders();
    Result<size_t> parseChunkedBody(const std::vector<utils::Byte>& buffer);
    std::string extractLine(const std::vector<utils::Byte>& buffer);
    static std::string trimOws(const std::string& s);
};

}  // namespace http

#endif