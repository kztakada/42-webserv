#ifndef HTTP_HTTP_REQUEST_HPP_
#define HTTP_HTTP_REQUEST_HPP_

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "http/content_types.hpp"
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
    // DoS耐性のための 防御的デフォルト としてよく使われる “業界の相場”
    // に寄せた値
    static const size_t kDefaultMaxRequestLineLength = 8192;
    static const size_t kDefaultMaxHeaderBytes = 16384;
    static const size_t kDefaultMaxHeaderCount = 100;
    static const size_t kDefaultMaxBodyBytes = 1024 * 1024;

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
    class BodySink
    {
       public:
        virtual ~BodySink() {}
        virtual Result<void> write(const utils::Byte* data, size_t len) = 0;
    };

    // buffer 先頭からの消費バイト数を返す。
    // body は HttpRequest では保持せず、必要なら sink に逐次出力する。
    // sink==NULL の場合、body は検証のみ行って破棄する。
    // stop_after_headers=true
    // の場合、ヘッダーセクション終端(CRLF)を読んだ時点で 必ず return
    // し、同一バッファ内に混在している body は次回呼び出しに回す。
    // ルーティング結果に応じて Limits(max_body_bytes) を差し替える用途を想定。
    Result<size_t> parse(const std::vector<utils::Byte>& buffer,
        BodySink* sink = NULL, bool stop_after_headers = false);
    Result<size_t> parse(const utils::Byte* data, size_t len, BodySink* sink,
        bool stop_after_headers);

    bool isParseComplete() const;
    bool isHeaderComplete() const;
    bool hasParseError() const;
    HttpStatus getParseErrorStatus() const;

    // ペイロードが制限を超えた場合、HTTPとしては正しい形でも
    // アプリケーションとしては 413 を返す必要がある。
    // keep-alive を維持するため、パーサはボディを最後まで読み飛ばして
    // ストリーム同期を保つ（phase_ は kComplete になり得る）。
    bool isPayloadTooLarge() const;

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
    bool isChunkedEncoding() const;
    bool hasBody() const;
    size_t getDecodedBodyBytes() const;

    // Content-Type
    // headers の Content-Type をパースした結果を返す。
    // 未指定/未知の場合は UNKNOWN。
    ContentType getContentType() const;
    bool hasContentTypeParam(const std::string& key_lowercase) const;
    std::string getContentTypeParam(const std::string& key_lowercase) const;

    // Keep-Alive
    // 判定（HTTP/1.1はデフォルトKeep-Alive、HTTP/1.0はデフォルトclose）
    // Connection: close / keep-alive を解釈した結果を返す。
    bool shouldKeepAlive() const;

    // 防御的制限（DoS耐性）
    void setLimits(const Limits& limits);
    const Limits& getLimits() const;

   private:
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

    // request-line 前の先頭空行(CRLF)を許容する回数を管理する。
    // RFC 7230 Section 3.5 の "at least one empty line"
    // に合わせ、1回までは無視。 連続した空行は不正入力として 400 にする。
    size_t leading_empty_lines_;

    // リクエストライン
    HttpMethod method_;
    std::string method_string_;
    std::string path_;
    std::string query_string_;
    int minor_version_;

    // ヘッダセクション
    HeaderMap headers_;

    // Content-Type（ヘッダ確定時に一括解析）
    ContentType content_type_;
    std::map<std::string, std::string> content_type_params_;

    // メッセージボディ
    BodyFraming body_framing_;
    size_t decoded_body_bytes_;

    // content-lengthタイプの管理
    unsigned long content_length_remaining_;

    // chunkタイプの実デコード管理
    ChunkPhase chunk_phase_;
    unsigned long chunk_bytes_remaining_;

    bool should_keep_alive_;

    // ボディが limits_.max_body_bytes を超えた場合に立つ。
    // parse エラーとして扱わず、残りを読み飛ばして complete にする。
    bool payload_too_large_;

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
    Result<void> validateHeaders(bool skip_body_size_check);
    Result<size_t> parseChunkedBody(
        const utils::Byte* data, size_t len, BodySink* sink);
    std::string extractLine(const utils::Byte* data, size_t len);
    static std::string trimOws(const std::string& s);
};

}  // namespace http

#endif