#include "http/http_request.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>

#include "http/syntax.hpp"

using namespace http;
using namespace utils::result;

static bool isLws(char c) { return c == ' ' || c == '\t'; }

static bool isTchar(unsigned char c)
{
    // RFC 9110: tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" /
    //                 "-" / "." / "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA
    if (std::isalnum(c))
        return true;
    switch (c)
    {
        case '!':
        case '#':
        case '$':
        case '%':
        case '&':
        case '\'':
        case '*':
        case '+':
        case '-':
        case '.':
        case '^':
        case '_':
        case '`':
        case '|':
        case '~':
            return true;
        default:
            return false;
    }
}

static std::string toLowerAscii(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] =
            static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

static bool splitCommaSeparatedValues(const std::vector<std::string>& values,
    std::vector<std::string>& out_tokens)
{
    out_tokens.clear();
    for (size_t i = 0; i < values.size(); ++i)
    {
        const std::string& v = values[i];
        std::string cur;
        for (size_t j = 0; j < v.size(); ++j)
        {
            char c = v[j];
            if (c == ',')
            {
                out_tokens.push_back(cur);
                cur.clear();
            }
            else
            {
                cur += c;
            }
        }
        out_tokens.push_back(cur);
    }
    return true;
}

static bool containsOws(const std::string& s)
{
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (isLws(s[i]))
            return true;
    }
    return false;
}

static bool parseUnsignedLongStrict(const std::string& s, unsigned long* out)
{
    if (out == NULL)
        return false;
    if (s.empty())
        return false;
    char* end = NULL;
    unsigned long n = std::strtoul(s.c_str(), &end, 10);
    if (end == s.c_str() || (end != NULL && *end != '\0'))
        return false;
    *out = n;
    return true;
}

static bool wouldExceedLimit(size_t already, size_t add, size_t limit)
{
    if (limit == 0)
        return false;
    if (already > limit)
        return true;
    return add > (limit - already);
}

HttpRequest::Limits::Limits()
    : max_request_line_length(HttpRequest::kDefaultMaxRequestLineLength),
      max_header_bytes(HttpRequest::kDefaultMaxHeaderBytes),
      max_header_count(HttpRequest::kDefaultMaxHeaderCount),
      max_body_bytes(HttpRequest::kDefaultMaxBodyBytes)
{
}

HttpRequest::HttpRequest()
    : method_(HttpMethod::UNKNOWN),
      method_string_(),
      path_(),
      query_string_(),
      minor_version_(1),
      headers_(),
      phase_(kRequestLine),
      parse_error_status_(HttpStatus::OK),
      body_framing_(kNoBody),
      content_length_remaining_(0),
      cursor_(0),
      chunk_phase_(kChunkSizeLine),
      chunk_bytes_remaining_(0),
      decoded_body_bytes_(0),
      should_keep_alive_(true),
      limits_(),
      header_bytes_parsed_(0),
      header_count_(0)
{
}

HttpRequest::HttpRequest(const HttpRequest& rhs)
    : method_(rhs.method_),
      method_string_(rhs.method_string_),
      path_(rhs.path_),
      query_string_(rhs.query_string_),
      minor_version_(rhs.minor_version_),
      headers_(rhs.headers_),
      phase_(rhs.phase_),
      parse_error_status_(rhs.parse_error_status_),
      body_framing_(rhs.body_framing_),
      content_length_remaining_(rhs.content_length_remaining_),
      cursor_(rhs.cursor_),
      chunk_phase_(rhs.chunk_phase_),
      chunk_bytes_remaining_(rhs.chunk_bytes_remaining_),
      decoded_body_bytes_(rhs.decoded_body_bytes_),
      limits_(rhs.limits_),
      header_bytes_parsed_(rhs.header_bytes_parsed_),
      header_count_(rhs.header_count_)
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
        phase_ = rhs.phase_;
        parse_error_status_ = rhs.parse_error_status_;
        body_framing_ = rhs.body_framing_;
        content_length_remaining_ = rhs.content_length_remaining_;
        cursor_ = rhs.cursor_;
        chunk_phase_ = rhs.chunk_phase_;
        chunk_bytes_remaining_ = rhs.chunk_bytes_remaining_;
        decoded_body_bytes_ = rhs.decoded_body_bytes_;

        limits_ = rhs.limits_;
        header_bytes_parsed_ = rhs.header_bytes_parsed_;
        header_count_ = rhs.header_count_;
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
    return HttpSyntax::kHttpVersionPrefix + HttpSyntax::kExpectMajorVersion +
           (minor_version_ == 0 ? "0" : "1");
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

bool HttpRequest::isChunkedEncoding() const
{
    return body_framing_ == kChunked;
}

bool HttpRequest::hasBody() const { return body_framing_ != kNoBody; }

size_t HttpRequest::getDecodedBodyBytes() const { return decoded_body_bytes_; }

void HttpRequest::setLimits(const Limits& limits) { limits_ = limits; }

const HttpRequest::Limits& HttpRequest::getLimits() const { return limits_; }

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

std::string HttpRequest::extractLine(const utils::Byte* data, size_t len)
{
    if (data == NULL)
        return std::string();
    if (cursor_ >= len)
        return std::string();
    if (len - cursor_ < 2)
        return std::string();

    for (size_t i = cursor_; i + 1 < len; ++i)
    {
        if (data[i] == '\r' && data[i + 1] == '\n')
        {
            const size_t len = (i + 2) - cursor_;
            std::string line;
            line.reserve(len);
            for (size_t j = cursor_; j < i + 2; ++j)
                line += static_cast<char>(data[j]);
            cursor_ = i + 2;
            return line;
        }
    }
    return std::string();
}

Result<void> HttpRequest::parseRequestLine(const std::string& line)
{
    // line includes CRLF
    if (line.size() < 2 || line.substr(line.size() - 2) != HttpSyntax::kCrlf)
        return Result<void>(ERROR, "request line missing CRLF");

    std::string s = line.substr(0, line.size() - 2);

    // RFC 9112: request-line = method SP request-target SP HTTP-version CRLF
    // SP は区切りとして厳密に 2つ、かつ連続しないことを要求する
    const std::string::size_type sp1 = s.find(' ');
    if (sp1 == std::string::npos || sp1 == 0)
        return Result<void>(ERROR, "invalid request line");
    if (sp1 + 1 >= s.size() || s[sp1 + 1] == ' ')
        return Result<void>(ERROR, "invalid request line");

    const std::string::size_type sp2 = s.find(' ', sp1 + 1);
    if (sp2 == std::string::npos || sp2 == sp1 + 1)
        return Result<void>(ERROR, "invalid request line");
    if (sp2 + 1 >= s.size() || s[sp2 + 1] == ' ')
        return Result<void>(ERROR, "invalid request line");
    if (s.find(' ', sp2 + 1) != std::string::npos)
        return Result<void>(ERROR, "invalid request line");

    const std::string method = s.substr(0, sp1);
    const std::string target = s.substr(sp1 + 1, sp2 - (sp1 + 1));
    const std::string version = s.substr(sp2 + 1);

    Result<void> r;
    r = parseMethod(method);
    if (!r.isOk())
        return r;
    r = parseRequestTarget(target);
    if (!r.isOk())
        return r;
    r = parseVersion(version);
    if (!r.isOk())
        return r;

    return Result<void>();
}

Result<void> HttpRequest::parseHeaderLine(const std::string& line)
{
    if (line.size() < 2 || line.substr(line.size() - 2) != HttpSyntax::kCrlf)
        return Result<void>(ERROR, "header line missing CRLF");

    std::string s = line.substr(0, line.size() - 2);
    const std::string::size_type colon_pos = s.find(':');
    if (colon_pos == std::string::npos)
        return Result<void>(ERROR, "invalid header line");

    std::string name = s.substr(0, colon_pos);
    std::string value =
        (colon_pos + 1 < s.size()) ? s.substr(colon_pos + 1) : std::string();

    // RFC 9110: field-name = token
    if (name.empty())
        return Result<void>(ERROR, "empty header name");
    for (size_t i = 0; i < name.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (!isTchar(c))
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

    // RFC 9110: method = token
    for (size_t i = 0; i < method.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(method[i]);
        if (!isTchar(c))
            return Result<void>(ERROR, "invalid method token");
    }
    method_string_ = method;
    method_ = HttpMethod::fromString(method);
    return Result<void>();
}

static bool parseAbsoluteFormUri(
    const std::string& target, std::string& out_path, std::string& out_query)
{
    // absolute-form = absolute-URI (RFC 9112)
    // Minimal parsing: scheme "://" authority [ "/" ... ] [ "?" query ]
    const std::string::size_type scheme_sep = target.find("://");
    if (scheme_sep == std::string::npos || scheme_sep == 0)
        return false;

    const std::string::size_type authority_start = scheme_sep + 3;
    if (authority_start >= target.size())
        return false;

    const std::string::size_type first_slash =
        target.find('/', authority_start);
    const std::string::size_type first_q = target.find('?', authority_start);

    if (first_slash == std::string::npos ||
        (first_q != std::string::npos && first_q < first_slash))
    {
        // No path (or query appears before any '/'): treat as "/".
        out_path = "/";
        if (first_q != std::string::npos)
            out_query = target.substr(first_q + 1);
        else
            out_query.clear();
        return true;
    }

    const std::string::size_type q_in_path = target.find('?', first_slash);
    if (q_in_path == std::string::npos)
    {
        out_path = target.substr(first_slash);
        out_query.clear();
    }
    else
    {
        out_path = target.substr(first_slash, q_in_path - first_slash);
        out_query = target.substr(q_in_path + 1);
    }

    if (out_path.empty())
        out_path = "/";
    if (out_path[0] != '/')
        return false;
    return true;
}

Result<void> HttpRequest::parseRequestTarget(const std::string& target)
{
    if (target.empty())
        return Result<void>(ERROR, "empty target");

    // RFC 9112: request-target には空白や制御文字が含まれてはならない
    for (size_t i = 0; i < target.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(target[i]);
        if (c <= 0x1F || c == 0x7F || c == ' ' || c == '\t')
            return Result<void>(ERROR, "invalid request-target");
    }

    // asterisk-form: only for OPTIONS
    if (method_ == HttpMethod::OPTIONS && target == "*")
    {
        path_ = "*";
        query_string_.clear();
        return Result<void>();
    }

    // authority-form: only for CONNECT
    if (method_ == HttpMethod::CONNECT)
    {
        // authority = host [":" port]
        // Reject obvious non-authority delimiters.
        if (target.find('/') != std::string::npos ||
            target.find('?') != std::string::npos)
            return Result<void>(ERROR, "invalid authority-form target");
        if (target == "*")
            return Result<void>(ERROR, "invalid authority-form target");

        path_ = target;
        query_string_.clear();
        return Result<void>();
    }

    // origin-form: absolute-path ["?" query]
    if (!target.empty() && target[0] == '/')
    {
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

        if (path_.empty() || path_[0] != '/')
            return Result<void>(ERROR, "invalid path");
        return Result<void>();
    }

    // absolute-form: absolute-URI
    {
        std::string abs_path;
        std::string abs_query;
        if (parseAbsoluteFormUri(target, abs_path, abs_query))
        {
            path_ = abs_path;
            query_string_ = abs_query;
            return Result<void>();
        }
    }

    return Result<void>(ERROR, "unsupported request-target form");
}

Result<void> HttpRequest::parseVersion(const std::string& version)
{
    const std::string expect_prefix =
        HttpSyntax::kHttpVersionPrefix + HttpSyntax::kExpectMajorVersion;
    if (version.size() != expect_prefix.size() + 1 ||
        version.substr(0, expect_prefix.size()) != expect_prefix)
    {
        parse_error_status_ = HttpStatus::HTTP_VERSION_NOT_SUPPORTED;
        return Result<void>(ERROR, "invalid http version");
    }
    char minor = version[expect_prefix.size()];
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

Result<void> HttpRequest::validateHeaders(bool skip_body_size_check)
{
    // RFC 9112: Host is required in HTTP/1.1
    if (minor_version_ == 1 && !hasHeader("Host"))
    {
        parse_error_status_ = HttpStatus::BAD_REQUEST;
        return Result<void>(ERROR, "missing Host header");
    }

    body_framing_ = kNoBody;
    content_length_remaining_ = 0;

    // Keep-Alive 判定
    // RFC 9112: HTTP/1.1 は持続接続がデフォルト。Connection: close で無効化。
    // HTTP/1.0 は非持続がデフォルト。Connection: keep-alive で有効化。
    should_keep_alive_ = (minor_version_ == 1);
    if (hasHeader("Connection"))
    {
        Result<const std::vector<std::string>&> c = getHeader("Connection");
        if (!c.isOk() || c.unwrap().empty())
        {
            parse_error_status_ = HttpStatus::BAD_REQUEST;
            return Result<void>(ERROR, "invalid Connection header");
        }

        std::vector<std::string> raw_tokens;
        splitCommaSeparatedValues(c.unwrap(), raw_tokens);
        for (size_t i = 0; i < raw_tokens.size(); ++i)
        {
            std::string t = trimOws(raw_tokens[i]);
            if (t.empty() || containsOws(t))
            {
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<void>(ERROR, "invalid Connection token");
            }
            t = toLowerAscii(t);
            if (t == "close")
                should_keep_alive_ = false;
            else if (t == "keep-alive")
                should_keep_alive_ = true;
        }
    }

    const bool has_te_header = hasHeader("Transfer-Encoding");
    const bool has_cl_header = hasHeader("Content-Length");

    // RFC 9112: Transfer-Encoding が存在する場合、フレーミングは TE
    // が優先される （Content-Length があっても無視する）

    // Transfer-Encoding
    if (has_te_header)
    {
        Result<const std::vector<std::string>&> te =
            getHeader("Transfer-Encoding");
        if (!te.isOk() || te.unwrap().empty())
        {
            parse_error_status_ = HttpStatus::BAD_REQUEST;
            return Result<void>(ERROR, "invalid Transfer-Encoding");
        }

        std::vector<std::string> raw_tokens;
        splitCommaSeparatedValues(te.unwrap(), raw_tokens);

        std::vector<std::string> codings;
        for (size_t i = 0; i < raw_tokens.size(); ++i)
        {
            std::string t = trimOws(raw_tokens[i]);
            if (t.empty() || containsOws(t))
            {
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<void>(ERROR, "invalid Transfer-Encoding token");
            }
            codings.push_back(toLowerAscii(t));
        }
        if (codings.empty())
        {
            parse_error_status_ = HttpStatus::BAD_REQUEST;
            return Result<void>(ERROR, "empty Transfer-Encoding");
        }

        bool has_chunked = false;
        size_t chunked_index = 0;
        for (size_t i = 0; i < codings.size(); ++i)
        {
            if (codings[i] == "chunked")
            {
                has_chunked = true;
                chunked_index = i;
            }
        }
        if (has_chunked && chunked_index != codings.size() - 1)
        {
            parse_error_status_ = HttpStatus::BAD_REQUEST;
            return Result<void>(ERROR, "chunked must be last transfer-coding");
        }

        // この実装では Transfer-Encoding は chunked のみサポートする
        for (size_t i = 0; i < codings.size(); ++i)
        {
            if (codings[i] != "chunked")
            {
                parse_error_status_ = HttpStatus::NOT_IMPLEMENTED;
                return Result<void>(ERROR, "unsupported transfer-coding");
            }
        }
        if (!has_chunked)
        {
            parse_error_status_ = HttpStatus::NOT_IMPLEMENTED;
            return Result<void>(ERROR, "Transfer-Encoding without chunked");
        }

        body_framing_ = kChunked;
        return Result<void>();
    }

    // Content-Length（複数値がすべて同じなら許容、違えば 400）
    if (has_cl_header)
    {
        Result<const std::vector<std::string>&> cl =
            getHeader("Content-Length");
        if (!cl.isOk() || cl.unwrap().empty())
        {
            parse_error_status_ = HttpStatus::BAD_REQUEST;
            return Result<void>(ERROR, "invalid Content-Length");
        }

        std::vector<std::string> raw_tokens;
        splitCommaSeparatedValues(cl.unwrap(), raw_tokens);
        if (raw_tokens.empty())
        {
            parse_error_status_ = HttpStatus::BAD_REQUEST;
            return Result<void>(ERROR, "empty Content-Length");
        }

        unsigned long first = 0;
        bool has_first = false;
        for (size_t i = 0; i < raw_tokens.size(); ++i)
        {
            std::string t = trimOws(raw_tokens[i]);
            unsigned long n = 0;
            if (!parseUnsignedLongStrict(t, &n))
            {
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<void>(ERROR, "invalid Content-Length");
            }
            if (!has_first)
            {
                first = n;
                has_first = true;
            }
            else if (n != first)
            {
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<void>(
                    ERROR, "multiple Content-Length values differ");
            }
        }

        content_length_remaining_ = first;
        if (!skip_body_size_check)
        {
            if (limits_.max_body_bytes != 0 &&
                content_length_remaining_ >
                    static_cast<unsigned long>(limits_.max_body_bytes))
            {
                parse_error_status_ = HttpStatus::PAYLOAD_TOO_LARGE;
                return Result<void>(ERROR, "body too large");
            }
        }
        body_framing_ =
            (content_length_remaining_ > 0) ? kContentLength : kNoBody;
    }
    else
    {
        body_framing_ = kNoBody;
    }

    return Result<void>();
}

bool HttpRequest::shouldKeepAlive() const { return should_keep_alive_; }

Result<size_t> HttpRequest::parseChunkedBody(
    const utils::Byte* data, size_t len, BodySink* sink)
{
    // RFC 9112: Transfer-Encoding: chunked
    // trailer は認識するが保持せず読み飛ばす（課題要件に合わせる）

    const size_t start_cursor = cursor_;

    // ここは増分パース前提：
    // - 入力が足りないときは break して呼び出し元に戻る
    // - 次回 parse() 呼び出しで、同じ状態（chunk_phase_ 等）から再開する
    while (phase_ == kBody)
    {
        if (chunk_phase_ == kChunkSizeLine)
        {
            // chunk-size行（16進数+optionalchunk-ext）を1行（CRLF）として読む
            // 例）5;key=value\r\n
            std::string line = extractLine(data, len);
            if (line.empty())
                break;  // データ不足

            if (line.size() < 2 ||
                line.substr(line.size() - 2) != HttpSyntax::kCrlf)
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, "chunk-size line missing CRLF");
            }

            // chunk-ext は今回無視する（; 以降を捨てる）
            std::string s = line.substr(0, line.size() - 2);
            std::string::size_type semi = s.find(';');
            std::string size_str =
                (semi == std::string::npos) ? s : s.substr(0, semi);
            size_str = trimOws(size_str);

            if (size_str.empty())
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, "empty chunk size");
            }

            // 　chunk-sizeがすべて数字かどうかのチェック
            for (size_t i = 0; i < size_str.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(size_str[i]);
                if (!std::isxdigit(c))
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::BAD_REQUEST;
                    return Result<size_t>(ERROR, "invalid chunk size");
                }
            }

            // chunk-sizeをstringからunsigned intに変換
            char* end = NULL;
            unsigned long n = std::strtoul(size_str.c_str(), &end, 16);
            if (end == size_str.c_str() || (end != NULL && *end != '\0'))
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, "invalid chunk size parse");
            }

            chunk_bytes_remaining_ = n;
            if (chunk_bytes_remaining_ == 0)
            {
                // 0 なら last-chunk。続いて trailer（空行まで）を消費する
                chunk_phase_ = kChunkTrailer;
            }
            else
            {
                // 非 0 なら、そのサイズ分の chunk-data を読み込む
                chunk_phase_ = kChunkData;
            }
        }
        else if (chunk_phase_ == kChunkData)
        {
            // chunk-dataを可能な限りボディにコピーする（足りない分は次回に回す）

            // バッファの中でchunk-data以降の文字数を保存
            const size_t available = (cursor_ < len) ? (len - cursor_) : 0;
            if (available == 0)
                break;

            // 取得可能なchunk-dataを取得
            const size_t take =
                (available < chunk_bytes_remaining_)
                    ? available
                    : static_cast<size_t>(chunk_bytes_remaining_);

            if (wouldExceedLimit(
                    decoded_body_bytes_, take, limits_.max_body_bytes))
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::PAYLOAD_TOO_LARGE;
                return Result<size_t>(ERROR, "body too large");
            }

            if (sink != NULL)
            {
                Result<void> w = sink->write(data + cursor_, take);
                if (!w.isOk())
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::SERVER_ERROR;
                    return Result<size_t>(ERROR, w.getErrorMessage());
                }
            }
            decoded_body_bytes_ += take;

            cursor_ += take;
            chunk_bytes_remaining_ -= take;

            if (chunk_bytes_remaining_ == 0)
            {
                // chunk-data を読み切ったら、直後に必ず CRLF が来る（\r\n）
                chunk_phase_ = kChunkDataCrlf;
            }
        }
        else if (chunk_phase_ == kChunkDataCrlf)
        {
            // chunk-data の直後にある CRLF を検証して消費する（\r\n）
            if (len - cursor_ < 2)
                break;
            if (data[cursor_] != '\r' || data[cursor_ + 1] != '\n')
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, "missing CRLF after chunk data");
            }
            cursor_ += 2;

            // 次の chunk-size 行へ
            chunk_phase_ = kChunkSizeLine;
        }
        else if (chunk_phase_ == kChunkTrailer)
        {
            // trailerは中身（フィールド名と値）は無視するが、終わり（空行）までは正しく読み飛ばす
            // 実装が複雑になり、今回の課題では求められていないから

            // trailer-part = *( field-line CRLF )
            // 最終 CRLF まで読み飛ばす
            // （空行 "\r\n" が来たら chunked-body 終了）
            std::string line = extractLine(data, len);
            if (line.empty())
                break;

            // 　HTTPリクエストの最終ライン
            if (line == HttpSyntax::kCrlf)
            {
                // trailer の終端（空行）を見たのでメッセージは complete
                chunk_phase_ = kChunkDone;
                phase_ = kComplete;
                break;
            }

            // Trailerを読み飛ばす
            // ※この行自体は保持せず、ここまでで消費したことにする
            if (line.size() < 2 ||
                line.substr(line.size() - 2) != HttpSyntax::kCrlf)
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, "invalid trailer header line");
            }

            // RFC 9110: field-name = token
            // RFC 9112: obs-fold は廃止されているため拒否
            if (line[0] == ' ' || line[0] == '\t')
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, "obs-fold is not allowed");
            }

            const std::string s = line.substr(0, line.size() - 2);
            const std::string::size_type colon_pos = s.find(':');
            if (colon_pos == std::string::npos || colon_pos == 0)
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, "invalid trailer header line");
            }
            const std::string name = s.substr(0, colon_pos);
            for (size_t i = 0; i < name.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(name[i]);
                if (!isTchar(c))
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::BAD_REQUEST;
                    return Result<size_t>(ERROR, "invalid trailer header line");
                }
            }
        }
        else
        {
            // done
            // 本来ここには来ない想定だが、安全のため complete にして抜ける
            phase_ = kComplete;
            break;
        }
    }

    return Result<size_t>(cursor_ - start_cursor);
}

Result<size_t> HttpRequest::parse(const std::vector<utils::Byte>& buffer,
    BodySink* sink, bool stop_after_headers)
{
    const utils::Byte* p = (buffer.empty()) ? NULL : &buffer[0];
    return parse(p, buffer.size(), sink, stop_after_headers);
}

Result<size_t> HttpRequest::parse(const utils::Byte* data, size_t len,
    BodySink* sink, bool stop_after_headers)
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
            std::string line = extractLine(data, len);  // 「\r\n」を改行とする
            if (line.empty())
            {
                // CRLF が来ないまま巨大な request-line を溜め込ませない
                if (limits_.max_request_line_length != 0 &&
                    len > limits_.max_request_line_length + 2)
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::URI_TOO_LONG;
                    return Result<size_t>(ERROR, "request line too long");
                }
                break;  // データ不足
            }

            if (line.size() >= 2 && wouldExceedLimit(0, line.size() - 2,
                                        limits_.max_request_line_length))
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::URI_TOO_LONG;
                return Result<size_t>(ERROR, "request line too long");
            }

            Result<void> result = parseRequestLine(line);
            if (!result.isOk())
            {
                phase_ = kError;
                if (parse_error_status_ == HttpStatus::OK)
                    parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, result.getErrorMessage());
            }
            phase_ = kHeaderField;
        }
        else if (phase_ == kHeaderField)
        {
            // ヘッダー解析
            std::string line = extractLine(data, len);  // 「\r\n」を改行とする
            if (line.empty())
            {
                // CRLF が来ないまま巨大な header section を溜め込ませない
                if (wouldExceedLimit(
                        header_bytes_parsed_, len, limits_.max_header_bytes))
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::BAD_REQUEST;
                    return Result<size_t>(ERROR, "header section too large");
                }
                break;  // データ不足
            }

            if (wouldExceedLimit(header_bytes_parsed_, line.size(),
                    limits_.max_header_bytes))
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, "header section too large");
            }

            // RFC 9112: obs-fold は廃止。行頭 SP/HTAB の継続行は 400
            // で拒否する。
            if (line != HttpSyntax::kCrlf &&
                (line[0] == ' ' || line[0] == '\t'))
            {
                phase_ = kError;
                parse_error_status_ = HttpStatus::BAD_REQUEST;
                return Result<size_t>(ERROR, "obs-fold is not allowed");
            }

            if (line == HttpSyntax::kCrlf)
            {
                header_bytes_parsed_ += line.size();
                // ヘッダー終了
                Result<void> result = validateHeaders(stop_after_headers);
                if (!result.isOk())
                {
                    phase_ = kError;
                    return Result<size_t>(ERROR, result.getErrorMessage());
                }

                if (body_framing_ == kNoBody)
                    phase_ = kComplete;
                else
                    phase_ = kBody;

                if (stop_after_headers && phase_ == kBody)
                    break;
            }
            else
            {
                if (wouldExceedLimit(
                        header_count_, 1, limits_.max_header_count))
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::BAD_REQUEST;
                    return Result<size_t>(ERROR, "too many headers");
                }
                Result<void> result = parseHeaderLine(line);
                if (!result.isOk())
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::BAD_REQUEST;
                    return Result<size_t>(ERROR, result.getErrorMessage());
                }

                header_count_ += 1;
                header_bytes_parsed_ += line.size();
            }
        }
        else if (phase_ == kBody)
        {
            // ボディ解析
            if (body_framing_ == kChunked)
            {
                Result<size_t> result = parseChunkedBody(data, len, sink);
                if (!result.isOk())
                    return Result<size_t>(ERROR, result.getErrorMessage());

                // chunked は「データ不足で進捗0でも成功」で戻る。
                // このままwhileを続けると同じ入力で再試行し続けてハングするため、
                // 進捗がない場合はここで抜けて呼び出し側の追加入力を待つ。
                if (result.unwrap() == 0)
                    break;
            }
            else if (body_framing_ == kContentLength)
            {
                // Content-Length
                if (content_length_remaining_ == 0)
                {
                    phase_ = kComplete;
                    break;
                }

                const size_t available = (cursor_ < len) ? (len - cursor_) : 0;
                if (available == 0)
                    break;  // データ不足

                const size_t take =
                    (available < content_length_remaining_)
                        ? available
                        : static_cast<size_t>(content_length_remaining_);

                if (wouldExceedLimit(
                        decoded_body_bytes_, take, limits_.max_body_bytes))
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::PAYLOAD_TOO_LARGE;
                    return Result<size_t>(ERROR, "body too large");
                }

                if (sink != NULL)
                {
                    Result<void> w = sink->write(data + cursor_, take);
                    if (!w.isOk())
                    {
                        phase_ = kError;
                        parse_error_status_ = HttpStatus::SERVER_ERROR;
                        return Result<size_t>(ERROR, w.getErrorMessage());
                    }
                }
                decoded_body_bytes_ += take;
                cursor_ += take;
                content_length_remaining_ -= static_cast<unsigned long>(take);

                if (content_length_remaining_ == 0)
                    phase_ = kComplete;
                else
                    break;  // 追加入力待ち
            }
            else if (body_framing_ == kUntilClose)
            {
                // connection close まで読み続ける方式（現状は complete
                // にできない）
                const size_t available = (cursor_ < len) ? (len - cursor_) : 0;
                if (available == 0)
                    break;

                if (wouldExceedLimit(
                        decoded_body_bytes_, available, limits_.max_body_bytes))
                {
                    phase_ = kError;
                    parse_error_status_ = HttpStatus::PAYLOAD_TOO_LARGE;
                    return Result<size_t>(ERROR, "body too large");
                }

                if (sink != NULL)
                {
                    Result<void> w = sink->write(data + cursor_, available);
                    if (!w.isOk())
                    {
                        phase_ = kError;
                        parse_error_status_ = HttpStatus::SERVER_ERROR;
                        return Result<size_t>(ERROR, w.getErrorMessage());
                    }
                }
                decoded_body_bytes_ += available;
                cursor_ = len;
                break;
            }
            else
            {
                phase_ = kComplete;
                break;
            }
        }
    }

    return Result<size_t>(cursor_);
}

bool HttpRequest::isHeaderComplete() const
{
    return phase_ == kBody || phase_ == kComplete;
}
