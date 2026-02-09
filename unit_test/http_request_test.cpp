#include "http/http_request.hpp"

#include <gtest/gtest.h>

#include <sstream>

static std::vector<utils::Byte> toBytes(const char* s)
{
    std::vector<utils::Byte> out;
    if (s == NULL)
        return out;
    for (const char* p = s; *p != '\0'; ++p)
        out.push_back(static_cast<utils::Byte>(*p));
    return out;
}

class StringBodySink : public http::HttpRequest::BodySink
{
   public:
    StringBodySink() : out_() {}

    virtual utils::result::Result<void> write(
        const utils::Byte* data, size_t len)
    {
        if (data == NULL || len == 0)
            return utils::result::Result<void>();
        out_.append(reinterpret_cast<const char*>(data), len);
        return utils::result::Result<void>();
    }

    const std::string& str() const { return out_; }

   private:
    std::string out_;
};

// 簡単な成功例
TEST(HttpRequest, ParsesSimpleGetWithQueryAndHost)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("GET /hello?name=bob HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(buf.size(), r.unwrap());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());

    EXPECT_EQ(http::HttpMethod::GET, req.getMethod());
    EXPECT_EQ(std::string("/hello"), req.getPath());
    EXPECT_EQ(std::string("name=bob"), req.getQueryString());
    EXPECT_EQ(1, req.getMinorVersion());
    EXPECT_TRUE(req.hasHeader("Host"));
    EXPECT_EQ(static_cast<size_t>(0), req.getDecodedBodyBytes());
}

// HTTP 1.0なのでHost情報がなくてもよい
TEST(HttpRequest, Http10DoesNotRequireHost)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes("GET / HTTP/1.0\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(buf.size(), r.unwrap());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    EXPECT_EQ(0, req.getMinorVersion());
}

// HttpMethodクラスに定義されているメソッドを、HTTPリクエストとしてパースできる
// NOTE:
// CONNECT は authority-form のみ許可しているため、別テストで検証する
TEST(HttpRequest, ParsesAllDefinedMethodsWithOriginFormTarget)
{
    struct MethodCase
    {
        http::HttpMethod::Type type;
        const char* token;
    };

    static const MethodCase kCases[] = {
        {http::HttpMethod::GET, "GET"},
        {http::HttpMethod::POST, "POST"},
        {http::HttpMethod::PUT, "PUT"},
        {http::HttpMethod::DELETE, "DELETE"},
        {http::HttpMethod::HEAD, "HEAD"},
        {http::HttpMethod::OPTIONS, "OPTIONS"},
        {http::HttpMethod::PATCH, "PATCH"},
        {http::HttpMethod::TRACE, "TRACE"},
    };

    for (size_t i = 0; i < sizeof(kCases) / sizeof(kCases[0]); ++i)
    {
        http::HttpRequest req;
        std::string raw = std::string(kCases[i].token) + " /test HTTP/1.1\r\n" +
                          "Host: example.com\r\n";
        if (kCases[i].type == http::HttpMethod::POST ||
            kCases[i].type == http::HttpMethod::PUT ||
            kCases[i].type == http::HttpMethod::PATCH)
        {
            // 411 Length Required を避けるため、ボディ無しでも明示する
            raw += "Content-Length: 0\r\n";
        }
        raw += "\r\n";
        std::vector<utils::Byte> buf = toBytes(raw.c_str());
        utils::result::Result<size_t> r = req.parse(buf);

        EXPECT_TRUE(r.isOk()) << kCases[i].token;
        if (!r.isOk())
            continue;
        EXPECT_EQ(buf.size(), r.unwrap()) << kCases[i].token;
        EXPECT_TRUE(req.isParseComplete()) << kCases[i].token;
        EXPECT_FALSE(req.hasParseError()) << kCases[i].token;

        EXPECT_EQ(kCases[i].type, req.getMethod()) << kCases[i].token;
        EXPECT_EQ(std::string(kCases[i].token), req.getMethodString())
            << kCases[i].token;
        EXPECT_EQ(std::string("/test"), req.getPath()) << kCases[i].token;
        EXPECT_EQ(std::string(""), req.getQueryString()) << kCases[i].token;
        EXPECT_EQ(static_cast<size_t>(0), req.getDecodedBodyBytes())
            << kCases[i].token;
    }
}

// absolute-form: absolute-URI をパースできる
TEST(HttpRequest, ParsesAbsoluteFormTarget)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "GET http://example.com/hello?name=bob HTTP/1.1\r\n"
        "Host: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(buf.size(), r.unwrap());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());

    EXPECT_EQ(http::HttpMethod::GET, req.getMethod());
    EXPECT_EQ(std::string("/hello"), req.getPath());
    EXPECT_EQ(std::string("name=bob"), req.getQueryString());
}

// CONNECT: authority-form をパースできる
TEST(HttpRequest, ParsesConnectAuthorityFormTarget)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "CONNECT example.com:443 HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(buf.size(), r.unwrap());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());

    EXPECT_EQ(http::HttpMethod::CONNECT, req.getMethod());
    EXPECT_EQ(std::string("example.com:443"), req.getPath());
    EXPECT_EQ(std::string(""), req.getQueryString());
}

// OPTIONS: asterisk-form をパースできる
TEST(HttpRequest, ParsesOptionsAsteriskFormTarget)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("OPTIONS * HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(buf.size(), r.unwrap());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());

    EXPECT_EQ(http::HttpMethod::OPTIONS, req.getMethod());
    EXPECT_EQ(std::string("*"), req.getPath());
    EXPECT_EQ(std::string(""), req.getQueryString());
}

// 2回に分けて送信（増分パース / Chunked）
TEST(HttpRequest, ParsesChunkedBodyAndIgnoresTrailer)
{
    http::HttpRequest req;
    StringBodySink sink;

    // 2回に分けて流し込み（呼び出し側は「新しく受信した分だけ」を渡す想定）
    std::vector<utils::Byte> part1 = toBytes(
        "POST / HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: "
        "chunked\r\n\r\n"
        "4\r\nWi");
    utils::result::Result<size_t> r1 = req.parse(part1, &sink);
    EXPECT_TRUE(r1.isOk());

    // 残りを追加（chunk-data途中の続き + trailer + 終端）
    std::vector<utils::Byte> part2 =
        toBytes("ki\r\n5\r\npedia\r\n0\r\nX-Trailer: ignored\r\n\r\n");
    utils::result::Result<size_t> r2 = req.parse(part2, &sink);
    EXPECT_TRUE(r2.isOk());

    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    EXPECT_EQ(std::string("Wikipedia"), sink.str());
}

// 2回に分けて送信（増分パース / Content-Length）
TEST(HttpRequest, ParsesContentLengthBody)
{
    http::HttpRequest req;
    StringBodySink sink;

    // まず1回目の流し込み（呼び出し側は「新しく受信した分だけ」を渡す想定）
    std::vector<utils::Byte> part1 = toBytes(
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Wi");
    utils::result::Result<size_t> r1 = req.parse(part1, &sink);
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(part1.size(), r1.unwrap());
    EXPECT_FALSE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());

    // 次に2回目の流し込み
    std::vector<utils::Byte> part2 = toBytes("kipedia");
    utils::result::Result<size_t> r2 = req.parse(part2, &sink);
    EXPECT_TRUE(r2.isOk());
    EXPECT_EQ(part2.size(), r2.unwrap());

    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    EXPECT_EQ(std::string("Wikipedia"), sink.str());
}

// Content-Lengthが複数設定されている場合、入力値が一緒であれば受け入れる
TEST(HttpRequest, AcceptsMultipleContentLengthSameValue)
{
    http::HttpRequest req;
    StringBodySink sink;
    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 9\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Wikipedia");

    utils::result::Result<size_t> r = req.parse(buf, &sink);
    EXPECT_TRUE(r.isOk());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    EXPECT_EQ(std::string("Wikipedia"), sink.str());
}

// Content-Lengthの値が全て一致
TEST(HttpRequest, AcceptsContentLengthListSameValue)
{
    http::HttpRequest req;
    StringBodySink sink;
    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 9,9,9\r\n"
        "\r\n"
        "Wikipedia");

    utils::result::Result<size_t> r = req.parse(buf, &sink);
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(req.hasParseError());
    EXPECT_EQ(std::string("Wikipedia"), sink.str());
}

// errorハンドリング　-------------------------------------------------------------------

// HTTP 1.1なのにHost情報が抜けている
TEST(HttpRequest, MissingHostInHttp11IsBadRequest)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("GET / HTTP/1.1\r\nUser-Agent: x\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

// RFC 7230 Section 3.5: request-line の前の空行は少なくとも1回は無視できる
TEST(HttpRequest, AllowsSingleLeadingEmptyLineBeforeRequestLine)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("\r\nGET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(buf.size(), r.unwrap());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
}

// 空行だけ（CRLFを2回）を送ってヘッダ終端を作るのはリクエストとして不正
TEST(HttpRequest, RejectsTwoLeadingEmptyLinesAsBadRequest)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes("\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

// RFC 9110: field-name は token (tchar) のみ許可される
TEST(HttpRequest, RejectsInvalidHeaderFieldNameToken)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Foo@Bar: x\r\n"
        "\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

// RFC 9112: obs-fold (行頭SP/HTABの継続行) は廃止されているので 400
TEST(HttpRequest, RejectsObsFoldInHeaderSection)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "X-Test: a\r\n"
        "\tcontinued\r\n"
        "\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

// Content-lenthが複数設定されているが、値が一緒ではない
TEST(HttpRequest, RejectsMultipleContentLengthDifferentValue)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 9\r\n"
        "Content-Length: 8\r\n"
        "\r\n"
        "Wikipedia");

    utils::result::Result<size_t> r = req.parse(buf);
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

// RFC 9112: Transfer-Encoding がある場合はフレーミングは TE が優先される
TEST(HttpRequest, AcceptsTransferEncodingWithContentLengthAndIgnoresIt)
{
    http::HttpRequest req;
    StringBodySink sink;
    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Length: 999\r\n"
        "\r\n"
        "4\r\nWi"
        "ki\r\n"
        "5\r\npedia\r\n"
        "0\r\n\r\n");

    utils::result::Result<size_t> r = req.parse(buf, &sink);
    EXPECT_TRUE(r.isOk());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    EXPECT_EQ(std::string("Wikipedia"), sink.str());
}

TEST(HttpRequest, Http11DefaultsToKeepAlive)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n");
    utils::result::Result<size_t> r = req.parse(buf);
    EXPECT_TRUE(r.isOk());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_TRUE(req.shouldKeepAlive());
}

TEST(HttpRequest, Http11ConnectionCloseDisablesKeepAlive)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: close\r\n"
        "\r\n");
    utils::result::Result<size_t> r = req.parse(buf);
    EXPECT_TRUE(r.isOk());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.shouldKeepAlive());
}

TEST(HttpRequest, Http10DefaultsToClose)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "GET / HTTP/1.0\r\n"
        "\r\n");
    utils::result::Result<size_t> r = req.parse(buf);
    EXPECT_TRUE(r.isOk());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.shouldKeepAlive());
}

TEST(HttpRequest, Http10ConnectionKeepAliveEnablesKeepAlive)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "GET / HTTP/1.0\r\n"
        "Connection: keep-alive\r\n"
        "\r\n");
    utils::result::Result<size_t> r = req.parse(buf);
    EXPECT_TRUE(r.isOk());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_TRUE(req.shouldKeepAlive());
}

TEST(HttpRequest, RejectsInvalidMethodToken)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("G@T / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

TEST(HttpRequest, RejectsRequestLineWithMultipleSpaces)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("GET  / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

TEST(HttpRequest, RejectsUnsupportedHttpVersionWith505)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("GET / HTTP/1.2\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::HTTP_VERSION_NOT_SUPPORTED,
        req.getParseErrorStatus());
}

TEST(HttpRequest, RejectsPostWithoutLengthWith411)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("POST / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::LENGTH_REQUIRED, req.getParseErrorStatus());
}

// chunkedがリストの最後に来ていない(400)
TEST(HttpRequest, RejectsTransferEncodingChunkedNotLast)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked, gzip\r\n"
        "\r\n");

    utils::result::Result<size_t> r = req.parse(buf);
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

TEST(HttpRequest, DrainsTooLargeBodyByContentLengthAndMarksPayloadTooLarge)
{
    http::HttpRequest req;
    http::HttpRequest::Limits limits = req.getLimits();
    limits.max_body_bytes = 4;
    req.setLimits(limits);

    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\n"
        "abcde");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(buf.size(), r.unwrap());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    EXPECT_TRUE(req.isPayloadTooLarge());
}

TEST(HttpRequest, DrainsTooLargeBodyByChunkedAndMarksPayloadTooLarge)
{
    http::HttpRequest req;
    http::HttpRequest::Limits limits = req.getLimits();
    limits.max_body_bytes = 4;
    req.setLimits(limits);

    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: "
        "chunked\r\n\r\n"
        "5\r\n"
        "abcde\r\n"
        "0\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(buf.size(), r.unwrap());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    EXPECT_TRUE(req.isPayloadTooLarge());
}

// gzipなどのcodingは非対応（501）
TEST(HttpRequest, RejectsUnsupportedTransferCoding)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: gzip, chunked\r\n"
        "\r\n");

    utils::result::Result<size_t> r = req.parse(buf);
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::NOT_IMPLEMENTED, req.getParseErrorStatus());
}

// Content-Lengthの値が一緒でない
TEST(HttpRequest, RejectsContentLengthListNotSame)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 9,8,8\r\n"
        "\r\n"
        "Wikipedia");

    utils::result::Result<size_t> r = req.parse(buf);
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

// authority-form は CONNECT 以外ではエラー
TEST(HttpRequest, RejectsAuthorityFormTargetWhenNotConnect)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("GET example.com:443 HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

// asterisk-form は OPTIONS 以外ではエラー
TEST(HttpRequest, RejectsAsteriskFormTargetWhenNotOptions)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf =
        toBytes("GET * HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

TEST(HttpRequest, RejectsTooLongRequestLine)
{
    http::HttpRequest req;
    http::HttpRequest::Limits limits = req.getLimits();
    limits.max_request_line_length = 16;
    limits.max_header_bytes = 1024;
    limits.max_header_count = 10;
    req.setLimits(limits);

    std::vector<utils::Byte> buf = toBytes(
        "GET /this-is-way-too-long HTTP/1.1\r\nHost: example.com\r\n\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::URI_TOO_LONG, req.getParseErrorStatus());
}

TEST(HttpRequest, RejectsTooManyHeaders)
{
    http::HttpRequest req;
    http::HttpRequest::Limits limits = req.getLimits();
    limits.max_request_line_length = 1024;
    limits.max_header_bytes = 4096;
    limits.max_header_count = 2;
    req.setLimits(limits);

    std::vector<utils::Byte> buf = toBytes(
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "X-A: 1\r\n"
        "X-B: 2\r\n"
        "\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

TEST(HttpRequest, RejectsTooLargeHeaderSection)
{
    http::HttpRequest req;
    http::HttpRequest::Limits limits = req.getLimits();
    limits.max_request_line_length = 1024;
    limits.max_header_bytes = 20;
    limits.max_header_count = 10;
    req.setLimits(limits);

    std::vector<utils::Byte> buf = toBytes(
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n");
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

TEST(HttpRequest, AcceptsManyHeadersWithDefaultLimits)
{
    http::HttpRequest req;
    http::HttpRequest::Limits limits = req.getLimits();

    EXPECT_EQ(
        8192ul, static_cast<unsigned long>(limits.max_request_line_length));
    EXPECT_EQ(16384ul, static_cast<unsigned long>(limits.max_header_bytes));
    EXPECT_EQ(100ul, static_cast<unsigned long>(limits.max_header_count));

    std::ostringstream oss;
    oss << "GET / HTTP/1.1\r\n";
    oss << "Host: example.com\r\n";

    // Host を含めて max_header_count ちょうどになるように追加
    for (size_t i = 0; i < limits.max_header_count - 1; ++i)
    {
        oss << "X-" << i << ": a\r\n";
    }
    oss << "\r\n";

    std::string s = oss.str();
    std::vector<utils::Byte> buf = toBytes(s.c_str());
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_TRUE(r.isOk());
    if (!r.isOk())
        return;
    EXPECT_EQ(buf.size(), r.unwrap());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
}

TEST(HttpRequest, RejectsTooManyHeadersWithDefaultLimits)
{
    http::HttpRequest req;
    http::HttpRequest::Limits limits = req.getLimits();

    std::ostringstream oss;
    oss << "GET / HTTP/1.1\r\n";
    oss << "Host: example.com\r\n";

    // Host を含めて max_header_count を超えるように追加
    for (size_t i = 0; i < limits.max_header_count; ++i)
    {
        oss << "X-" << i << ": a\r\n";
    }
    oss << "\r\n";

    std::string s = oss.str();
    std::vector<utils::Byte> buf = toBytes(s.c_str());
    utils::result::Result<size_t> r = req.parse(buf);

    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}

TEST(HttpRequest, RejectsInvalidTrailerHeaderFieldNameToken)
{
    http::HttpRequest req;

    std::vector<utils::Byte> buf = toBytes(
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "0\r\n"
        "Foo@Bar: x\r\n"
        "\r\n");

    utils::result::Result<size_t> r = req.parse(buf);
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(req.hasParseError());
    EXPECT_EQ(http::HttpStatus::BAD_REQUEST, req.getParseErrorStatus());
}