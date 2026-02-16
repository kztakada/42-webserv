#include "http/http_response_encoder.hpp"

#include <sstream>

#include "http/header.hpp"
#include "http/syntax.hpp"

namespace http
{

HttpResponseEncoder::HttpResponseEncoder(const Options& options)
    : options_(options),
      body_mode_(kNoBody),
      decided_(false),
      should_close_connection_(false),
      expected_content_length_(0),
      body_bytes_sent_(0)
{
}

HttpResponseEncoder::~HttpResponseEncoder() {}

HttpResponseEncoder::BodyMode HttpResponseEncoder::bodyMode() const
{
    return body_mode_;
}

bool HttpResponseEncoder::shouldCloseConnection() const
{
    return should_close_connection_;
}

void HttpResponseEncoder::decide_(HttpResponse& response)
{
    if (decided_)
        return;

    decided_ = true;
    body_bytes_sent_ = 0;
    expected_content_length_ = 0;

    const int minor = options_.request_minor_version;

    // --- ボディ有無の決定（メソッド/ステータス）---
    if (options_.request_is_head ||
        isBodyForbiddenStatus_(response.getStatus()))
    {
        body_mode_ = kNoBody;
    }
    else if (response.hasExpectedContentLength())
    {
        body_mode_ = kContentLength;
        expected_content_length_ = response.expectedContentLength();
    }
    else
    {
        if (minor >= 1)
        {
            body_mode_ = kChunked;
        }
        else
        {
            // HTTP/1.0 では chunked が使えないため close-delimited
            body_mode_ = kCloseDelimited;
            should_close_connection_ = true;
        }
    }

    // --- Connection 制御 ---
    if (minor >= 1)
    {
        if (!options_.request_should_keep_alive)
        {
            should_close_connection_ = true;
        }
    }
    else
    {
        // HTTP/1.0 はデフォルト close。
        // keep-alive を維持するには Content-Length が必要。
        if (options_.request_should_keep_alive)
        {
            if (body_mode_ == kContentLength || body_mode_ == kNoBody)
            {
                // keep-alive 可能
            }
            else
            {
                should_close_connection_ = true;
            }
        }
        else
        {
            should_close_connection_ = true;
        }
    }
}

Result<std::vector<utils::Byte> > HttpResponseEncoder::encodeHeader(
    HttpResponse& response)
{
    decide_(response);

    // 送出バージョンは request に合わせる（response の値は上書きしない方針）
    const std::string version =
        (options_.request_minor_version >= 1) ? "HTTP/1.1" : "HTTP/1.0";

    std::vector<utils::Byte> out;

    // Status-Line
    {
        std::ostringstream oss;
        oss << version << " " << response.getStatus().toInt() << " "
            << reasonOrDefault_(response) << HttpSyntax::kCrlf;
        appendString_(out, oss.str());
    }

    // ヘッダーのコピーを作る（必要に応じて追加/削除）
    HeaderMap headers = response.getHeaders();

    // Transfer-Encoding / Content-Length 調整
    if (body_mode_ == kChunked)
    {
        // RFC: chunked の場合 Content-Length は送らない
        HeaderMap::iterator it =
            headers.find(HeaderName(HeaderName::CONTENT_LENGTH).toString());
        if (it != headers.end())
            headers.erase(it);
        headers[HeaderName(HeaderName::TRANSFER_ENCODING).toString()].clear();
        headers[HeaderName(HeaderName::TRANSFER_ENCODING).toString()].push_back(
            "chunked");
    }

    // Connection
    if (options_.request_minor_version >= 1)
    {
        if (should_close_connection_)
        {
            headers[HeaderName(HeaderName::CONNECTION).toString()].clear();
            headers[HeaderName(HeaderName::CONNECTION).toString()].push_back(
                "close");
        }
    }
    else
    {
        if (should_close_connection_)
        {
            headers[HeaderName(HeaderName::CONNECTION).toString()].clear();
            headers[HeaderName(HeaderName::CONNECTION).toString()].push_back(
                "close");
        }
        else
        {
            headers[HeaderName(HeaderName::CONNECTION).toString()].clear();
            headers[HeaderName(HeaderName::CONNECTION).toString()].push_back(
                "keep-alive");
        }
    }

    // Headers
    for (HeaderMap::const_iterator it = headers.begin(); it != headers.end();
        ++it)
    {
        const std::string& name = it->first;
        const std::vector<std::string>& values = it->second;
        for (size_t i = 0; i < values.size(); ++i)
        {
            appendString_(out, name);
            appendString_(out, ": ");
            appendString_(out, values[i]);
            appendCrlf_(out);
        }
    }

    // End of headers
    appendCrlf_(out);

    Result<void> marked = response.markHeadersFlushed();
    if (marked.isError())
        return Result<std::vector<utils::Byte> >(
            ERROR, marked.getErrorMessage());

    if (body_mode_ == kNoBody)
    {
        Result<void> c = response.markComplete();
        if (c.isError())
            return Result<std::vector<utils::Byte> >(
                ERROR, c.getErrorMessage());
    }

    return out;
}

Result<std::vector<utils::Byte> > HttpResponseEncoder::encodeBodyChunk(
    HttpResponse& response, const utils::Byte* data, size_t len)
{
    decide_(response);

    if (response.isComplete())
        return std::vector<utils::Byte>();

    if (body_mode_ == kNoBody)
    {
        if (len == 0)
            return std::vector<utils::Byte>();
        return Result<std::vector<utils::Byte> >(ERROR, "body is not allowed");
    }

    if (data == NULL && len != 0)
        return Result<std::vector<utils::Byte> >(ERROR, "invalid body pointer");

    if (body_mode_ == kContentLength)
    {
        const unsigned long after =
            body_bytes_sent_ + static_cast<unsigned long>(len);
        if (after > expected_content_length_)
            return Result<std::vector<utils::Byte> >(
                ERROR, "body exceeds Content-Length");
        body_bytes_sent_ = after;

        std::vector<utils::Byte> out;
        out.insert(out.end(), data, data + len);
        return out;
    }

    if (body_mode_ == kChunked)
    {
        std::vector<utils::Byte> out;
        const std::string head = toHex_(len) + HttpSyntax::kCrlf;
        appendString_(out, head);
        out.insert(out.end(), data, data + len);
        appendCrlf_(out);
        return out;
    }

    // close-delimited: そのまま
    std::vector<utils::Byte> out;
    out.insert(out.end(), data, data + len);
    return out;
}

Result<std::vector<utils::Byte> > HttpResponseEncoder::encodeEof(
    HttpResponse& response)
{
    decide_(response);

    if (response.isComplete())
        return std::vector<utils::Byte>();

    if (body_mode_ == kContentLength)
    {
        if (body_bytes_sent_ != expected_content_length_)
            return Result<std::vector<utils::Byte> >(
                ERROR, "body length mismatch");
    }

    std::vector<utils::Byte> out;
    if (body_mode_ == kChunked)
    {
        appendString_(out, "0\r\n\r\n");
    }

    Result<void> c = response.markComplete();
    if (c.isError())
        return Result<std::vector<utils::Byte> >(ERROR, c.getErrorMessage());

    return out;
}

bool HttpResponseEncoder::isBodyForbiddenStatus_(HttpStatus status)
{
    const int code = status.toInt();
    if (code >= 100 && code < 200)
        return true;
    if (code == 204 || code == 304)
        return true;
    return false;
}

std::string HttpResponseEncoder::reasonOrDefault_(const HttpResponse& response)
{
    const std::string& r = response.getReasonPhrase();
    if (!r.empty())
        return r;
    return response.getStatus().getMessage();
}

void HttpResponseEncoder::appendString_(
    std::vector<utils::Byte>& out, const std::string& s)
{
    out.reserve(out.size() + s.size());
    for (size_t i = 0; i < s.size(); ++i)
        out.push_back(static_cast<utils::Byte>(s[i]));
}

void HttpResponseEncoder::appendCrlf_(std::vector<utils::Byte>& out)
{
    out.push_back('\r');
    out.push_back('\n');
}

std::string HttpResponseEncoder::toHex_(size_t n)
{
    std::ostringstream oss;
    oss << std::hex << n;
    return oss.str();
}

}  // namespace http
