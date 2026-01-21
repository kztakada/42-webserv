#include "http/http_request.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>

using namespace http;
using namespace utils::result;

static bool isLws(char c) { return c == ' ' || c == '\t'; }

HttpRequest::HttpRequest()
    : method_(HttpMethod::UNKNOWN),
      method_string_(),
      path_(),
      query_string_(),
      minor_version_(1),
      headers_(),
      body_(),
      phase_(kRequestLine),
      parse_error_status_(HttpStatus::OK),
      is_chunked_(false),
      content_length_(0),
      cursor_(0)
{
}

HttpRequest::HttpRequest(const HttpRequest& rhs)
    : method_(rhs.method_),
      method_string_(rhs.method_string_),
      path_(rhs.path_),
      query_string_(rhs.query_string_),
      minor_version_(rhs.minor_version_),
      headers_(rhs.headers_),
      body_(rhs.body_),
      phase_(rhs.phase_),
      parse_error_status_(rhs.parse_error_status_),
      is_chunked_(rhs.is_chunked_),
      content_length_(rhs.content_length_),
      cursor_(rhs.cursor_)
{
}

HttpRequest& HttpRequest::operator=(const HttpRequest& rhs)
{
    if (this != &rhs)
    {
        method_ = rhs.method_;
        method_string_ = rhs.method_string_;
        path_ = rhs.path_;
        query_string_ = rhs.query_string_;
        minor_version_ = rhs.minor_version_;
        headers_ = rhs.headers_;
        body_ = rhs.body_;
        phase_ = rhs.phase_;
        parse_error_status_ = rhs.parse_error_status_;
        is_chunked_ = rhs.is_chunked_;
        content_length_ = rhs.content_length_;
        cursor_ = rhs.cursor_;
    }
    return *this;
}

HttpRequest::~HttpRequest() {}

bool HttpRequest::isParseComplete() const { return phase_ == kComplete; }

bool HttpRequest::hasParseError() const { return phase_ == kError; }

HttpStatus HttpRequest::getParseErrorStatus() const
{
    return parse_error_status_;
}

HttpMethod HttpRequest::getMethod() const { return method_; }

const std::string& HttpRequest::getMethodString() const
{
    return method_string_;
}

const std::string& HttpRequest::getPath() const { return path_; }

const std::string& HttpRequest::getQueryString() const { return query_string_; }

std::string HttpRequest::getHttpVersion() const
{
    return std::string("HTTP/1.") + (minor_version_ == 0 ? "0" : "1");
}

int HttpRequest::getMinorVersion() const { return minor_version_; }

const HeaderMap& HttpRequest::getHeaders() const { return headers_; }

Result<const std::vector<std::string>&> HttpRequest::getHeader(
    const std::string& name) const
{
    HeaderMap::const_iterator it = headers_.find(name);
    if (it == headers_.end())
        return Result<const std::vector<std::string>&>(
            ERROR, "header not found");
    return Result<const std::vector<std::string>&>(it->second);
}

bool HttpRequest::hasHeader(const std::string& name) const
{
    return headers_.find(name) != headers_.end();
}

const std::vector<utils::Byte>& HttpRequest::getBody() const { return body_; }

unsigned long HttpRequest::getBodySize() const
{
    return static_cast<unsigned long>(body_.size());
}

bool HttpRequest::isChunkedEncoding() const { return is_chunked_; }

std::string HttpRequest::trimOws(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && isLws(s[start]))
        ++start;
    size_t end = s.size();
    while (end > start && isLws(s[end - 1]))
        --end;
    return s.substr(start, end - start);
}

std::string HttpRequest::extractLine(const std::vector<utils::Byte>& buffer)
{
    if (cursor_ >= buffer.size())
        return std::string();
    if (buffer.size() - cursor_ < 2)
        return std::string();

    for (size_t i = cursor_; i + 1 < buffer.size(); ++i)
    {
        if (buffer[i] == '\r' && buffer[i + 1] == '\n')
        {
            const size_t len = (i + 2) - cursor_;
            std::string line;
            line.reserve(len);
            for (size_t j = cursor_; j < i + 2; ++j)
                line += static_cast<char>(buffer[j]);
            cursor_ = i + 2;
            return line;
        }
    }
    return std::string();
}

Result<void> HttpRequest::parseRequestLine(const std::string& line)
{
    // line includes CRLF
    if (line.size() < 2 || line.substr(line.size() - 2) != "\r\n")
        return Result<void>(ERROR, "request line missing CRLF");

    std::string s = line.substr(0, line.size() - 2);

    // Split by SP (single or multiple)
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c == ' ')
        {
            if (!cur.empty())
            {
                parts.push_back(cur);
                cur.clear();
            }
            while (i + 1 < s.size() && s[i + 1] == ' ')
                ++i;
        }
        else
        {
            cur += c;
        }
    }
    if (!cur.empty())
        parts.push_back(cur);

    if (parts.size() != 3)
        return Result<void>(ERROR, "invalid request line");

    Result<void> r;
    r = parseMethod(parts[0]);
    if (!r.isOk())
        return r;
    r = parsePath(parts[1]);
    if (!r.isOk())
        return r;
    r = parseVersion(parts[2]);
    if (!r.isOk())
        return r;

    return Result<void>();
}

Result<void> HttpRequest::parseHeaderLine(const std::string& line)
{
    if (line.size() < 2 || line.substr(line.size() - 2) != "\r\n")
        return Result<void>(ERROR, "header line missing CRLF");

    std::string s = line.substr(0, line.size() - 2);
    const std::string::size_type colon_pos = s.find(':');
    if (colon_pos == std::string::npos)
        return Result<void>(ERROR, "invalid header line");

    std::string name = s.substr(0, colon_pos);
    std::string value =
        (colon_pos + 1 < s.size()) ? s.substr(colon_pos + 1) : std::string();

    if (name.empty())
        return Result<void>(ERROR, "empty header name");
    for (size_t i = 0; i < name.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (std::iscntrl(c) || c == ' ' || c == '\t')
            return Result<void>(ERROR, "invalid header name");
    }

    value = trimOws(value);
    headers_[name].push_back(value);
    return Result<void>();
}

Result<void> HttpRequest::parseMethod(const std::string& method)
{
    if (method.empty())
        return Result<void>(ERROR, "empty method");
    method_string_ = method;
    method_ = HttpMethod::fromString(method);
    return Result<void>();
}

Result<void> HttpRequest::parsePath(const std::string& target)
{
    if (target.empty())
        return Result<void>(ERROR, "empty target");

    std::string::size_type q = target.find('?');
    if (q == std::string::npos)
    {
        path_ = target;
        query_string_.clear();
    }
    else
    {
        path_ = target.substr(0, q);
        query_string_ = target.substr(q + 1);
    }

    // first step: absolute-path should start with '/'
    if (path_.empty() || path_[0] != '/')
        return Result<void>(ERROR, "invalid path");
    return Result<void>();
}

Result<void> HttpRequest::parseVersion(const std::string& version)
{
    if (version.size() != 8 || version.substr(0, 7) != "HTTP/1.")
    {
        parse_error_status_ = HttpStatus::HTTP_VERSION_NOT_SUPPORTED;
        return Result<void>(ERROR, "invalid http version");
    }
    char minor = version[7];
    if (minor == '0')
        minor_version_ = 0;
    else if (minor == '1')
        minor_version_ = 1;
    else
    {
        parse_error_status_ = HttpStatus::HTTP_VERSION_NOT_SUPPORTED;
        return Result<void>(ERROR, "unsupported http version");
    }
    return Result<void>();
}

Result<void> HttpRequest::validateHeaders()
{
    // RFC 7230: Host is required in HTTP/1.1
    if (minor_version_ == 1 && !hasHeader("Host"))
    {
        parse_error_status_ = HttpStatus::BAD_REQUEST;
        return Result<void>(ERROR, "missing Host header");
    }

    // Transfer-Encoding: chunked
    if (hasHeader("Transfer-Encoding"))
    {
        Result<const std::vector<std::string>&> te =
            getHeader("Transfer-Encoding");
        if (te.isOk())
        {
            const std::vector<std::string>& vals = te.unwrap();
            for (size_t i = 0; i < vals.size(); ++i)
            {
                std::string v = vals[i];
                for (size_t j = 0; j < v.size(); ++j)
                    v[j] = static_cast<char>(
                        std::tolower(static_cast<unsigned char>(v[j])));
                if (v.find("chunked") != std::string::npos)
                    is_chunked_ = true;
            }
        }
    }

    if (hasHeader("Content-Length"))
    {
        Result<const std::vector<std::string>&> cl =
            getHeader("Content-Length");
        if (cl.isOk() && !cl.unwrap().empty())
        {
            const std::string& v = cl.unwrap()[0];
            char* end = NULL;
            unsigned long n = std::strtoul(v.c_str(), &end, 10);
            if (end == v.c_str() || (end != NULL && *end != '\0'))
            {
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<void>(ERROR, "invalid Content-Length");
            }
            content_length_ = n;
        }
    }

    if (is_chunked_ && content_length_ > 0)
    {
        parse_error_status_ = HttpStatus::BAD_REQUEST;
        return Result<void>(ERROR, "both chunked and content-length");
    }

    return Result<void>();
}

Result<size_t> HttpRequest::parse(const std::vector<utils::Byte>& buffer)
{
    // 呼び出し側が「今回渡したバッファの先頭から何バイト消費したか」で管理できるよう、
    // parse()のたびにカーソルは先頭(0)から開始する。
    cursor_ = 0;

    if (phase_ == kError)
    {
        return Result<size_t>(ERROR, "Already in error state");
    }

    if (phase_ == kComplete)
    {
        return Result<size_t>(0);  // すでに完了
    }

    // バッファから行単位で読み込み
    while (phase_ != kComplete && phase_ != kError)
    {
        if (phase_ == kRequestLine)
        {
            // リクエストライン解析
            std::string line = extractLine(buffer);
            if (line.empty())
                break;  // データ不足

            Result<void> result = parseRequestLine(line);
            if (!result.isOk())
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, result.getErrorMessage());
            }
            phase_ = kHeaderField;
        }
        else if (phase_ == kHeaderField)
        {
            // ヘッダー解析
            std::string line = extractLine(buffer);
            if (line.empty())
                break;  // データ不足

            if (line == "\r\n")
            {
                // ヘッダー終了
                Result<void> result = validateHeaders();
                if (!result.isOk())
                {
                    phase_ = kError;
                    return Result<size_t>(ERROR, result.getErrorMessage());
                }

                if (content_length_ > 0 || is_chunked_)
                {
                    phase_ = kBody;
                }
                else
                {
                    phase_ = kComplete;
                }
            }
            else
            {
                Result<void> result = parseHeaderLine(line);
                if (!result.isOk())
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::BAD_REQUEST;
                    return Result<size_t>(ERROR, result.getErrorMessage());
                }
            }
        }
        else if (phase_ == kBody)
        {
            // ボディ解析
            if (buffer.size() < cursor_ + content_length_)
            {
                break;  // データ不足
            }
            body_.assign(buffer.begin() + static_cast<std::ptrdiff_t>(cursor_),
                buffer.begin() +
                    static_cast<std::ptrdiff_t>(cursor_ + content_length_));
            cursor_ += content_length_;
            phase_ = kComplete;
        }
    }

    return Result<size_t>(cursor_);
}