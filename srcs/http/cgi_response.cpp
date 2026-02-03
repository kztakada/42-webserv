#include "http/cgi_response.hpp"

#include <cctype>
#include <cstdlib>

#include "http/http_response.hpp"

namespace http
{

CgiResponse::CgiResponse()
    : response_type_(kNotIdentified),
      headers_(),
      phase_(kHeader),
      line_buffer_()
{
}

CgiResponse::CgiResponse(const CgiResponse& rhs)
    : response_type_(rhs.response_type_),
      headers_(rhs.headers_),
      phase_(rhs.phase_),
      line_buffer_(rhs.line_buffer_)
{
}

CgiResponse& CgiResponse::operator=(const CgiResponse& rhs)
{
    if (this == &rhs)
        return *this;
    response_type_ = rhs.response_type_;
    headers_ = rhs.headers_;
    phase_ = rhs.phase_;
    line_buffer_ = rhs.line_buffer_;
    return *this;
}

CgiResponse::~CgiResponse() {}

void CgiResponse::reset()
{
    response_type_ = kNotIdentified;
    headers_.clear();
    phase_ = kHeader;
    line_buffer_.clear();
}

bool CgiResponse::isParseComplete() const { return phase_ == kComplete; }

CgiResponseType CgiResponse::getResponseType() const { return response_type_; }

const CgiResponse::HeaderVector& CgiResponse::getHeaders() const
{
    return headers_;
}

bool CgiResponse::ieq_(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        const unsigned char ac = static_cast<unsigned char>(a[i]);
        const unsigned char bc = static_cast<unsigned char>(b[i]);
        if (std::tolower(ac) != std::tolower(bc))
            return false;
    }
    return true;
}

std::string CgiResponse::trimOws_(const std::string& s)
{
    size_t start = 0;
    while (start < s.size())
    {
        const char c = s[start];
        if (c != ' ' && c != '\t')
            break;
        ++start;
    }

    size_t end = s.size();
    while (end > start)
    {
        const char c = s[end - 1];
        if (c != ' ' && c != '\t')
            break;
        --end;
    }

    return s.substr(start, end - start);
}

Result<std::string> CgiResponse::getHeader(const std::string& name) const
{
    for (HeaderVector::const_iterator it = headers_.begin();
        it != headers_.end(); ++it)
    {
        if (ieq_(it->first, name))
            return it->second;
    }
    return Result<std::string>(ERROR, std::string(), "header not found");
}

Result<void> CgiResponse::parseHeaderLine_(const std::string& line)
{
    const std::string::size_type pos = line.find(':');
    if (pos == std::string::npos)
        return Result<void>(ERROR, "invalid CGI header line");

    const std::string name = trimOws_(line.substr(0, pos));
    const std::string value = trimOws_(line.substr(pos + 1));

    if (name.empty())
        return Result<void>(ERROR, "invalid CGI header name");

    headers_.push_back(HeaderPair(name, value));
    return Result<void>();
}

Result<void> CgiResponse::finalize_()
{
    response_type_ = determineResponseType();
    if (response_type_ == kParseError)
    {
        phase_ = kError;
        return Result<void>(ERROR, "CGI response type parse error");
    }

    Result<void> v = validateCgiResponse();
    if (v.isError())
    {
        phase_ = kError;
        return v;
    }

    phase_ = kComplete;
    return Result<void>();
}

Result<size_t> CgiResponse::parse(const utils::Byte* data, size_t len)
{
    if (phase_ == kComplete)
        return 0;
    if (phase_ == kError)
        return Result<size_t>(ERROR, "CGI response is in error state");

    size_t consumed = 0;

    while (consumed < len)
    {
        const char c = static_cast<char>(data[consumed]);
        ++consumed;

        line_buffer_.push_back(c);

        if (c != '\n')
            continue;

        // 1行取り出し（末尾の\nは含まれている）
        std::string line = line_buffer_;
        line_buffer_.clear();

        // LFを落とし、CRがあれば落とす
        if (!line.empty() && line[line.size() - 1] == '\n')
            line.erase(line.size() - 1);
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        // 空行＝ヘッダ終端
        if (line.empty())
        {
            Result<void> fin = finalize_();
            if (fin.isError())
                return Result<size_t>(ERROR, fin.getErrorMessage());
            return consumed;
        }

        Result<void> ph = parseHeaderLine_(line);
        if (ph.isError())
        {
            phase_ = kError;
            response_type_ = kParseError;
            return Result<size_t>(ERROR, ph.getErrorMessage());
        }
    }

    return consumed;
}

CgiResponseType CgiResponse::determineResponseType() const
{
    // RFC 3875 Section 6 (rough classification)
    bool has_location = false;
    std::string location;
    bool has_content_type = false;

    for (HeaderVector::const_iterator it = headers_.begin();
        it != headers_.end(); ++it)
    {
        if (ieq_(it->first, "Location"))
        {
            has_location = true;
            location = it->second;
        }
        else if (ieq_(it->first, "Content-Type"))
        {
            has_content_type = true;
        }
    }

    if (!has_location)
        return kDocumentResponse;

    if (!location.empty() && location[0] == '/')
        return kLocalRedirect;

    if (has_content_type)
        return kClientRedirectWithDocument;

    return kClientRedirect;
}

Result<void> CgiResponse::validateCgiResponse() const
{
    // 必須チェックは控えめ（実運用で許容したい）。
    // ただしヘッダ終端までは読めている前提。
    return Result<void>();
}

HttpStatus CgiResponse::getHttpStatus() const
{
    // Status: 200 OK
    for (HeaderVector::const_iterator it = headers_.begin();
        it != headers_.end(); ++it)
    {
        if (!ieq_(it->first, "Status"))
            continue;

        const std::string& v = it->second;
        // 先頭の数値部分を読む
        const char* s = v.c_str();
        char* endp = NULL;
        long code = std::strtol(s, &endp, 10);
        if (endp == s || code < 100 || code > 999)
            break;
        return HttpStatus(static_cast<unsigned long>(code));
    }

    // Location がある場合はデフォルト 302
    for (HeaderVector::const_iterator it = headers_.begin();
        it != headers_.end(); ++it)
    {
        if (ieq_(it->first, "Location"))
            return http::HttpStatus::FOUND;
    }

    return http::HttpStatus::OK;
}

Result<std::string> CgiResponse::getLocalRedirectUriPath() const
{
    if (response_type_ != kLocalRedirect)
        return Result<std::string>(
            ERROR, std::string(), "not a local redirect");

    Result<std::string> loc = getHeader("Location");
    if (loc.isError())
        return loc;

    const std::string v = loc.unwrap();
    if (v.empty() || v[0] != '/')
        return Result<std::string>(
            ERROR, std::string(), "invalid local redirect location");

    return v;
}

Result<void> CgiResponse::applyToHttpResponse(
    http::HttpResponse& out_response) const
{
    Result<void> s = out_response.setStatus(getHttpStatus());
    if (s.isError())
        return s;

    // ヘッダ適用
    for (HeaderVector::const_iterator it = headers_.begin();
        it != headers_.end(); ++it)
    {
        const std::string& name = it->first;
        const std::string& value = it->second;

        if (ieq_(name, "Status"))
            continue;

        // Hop-by-hop と framing はサーバ側で制御
        if (ieq_(name, "Connection") || ieq_(name, "Transfer-Encoding") ||
            ieq_(name, "Keep-Alive") || ieq_(name, "Trailer") ||
            ieq_(name, "TE") || ieq_(name, "Upgrade"))
        {
            continue;
        }

        if (ieq_(name, "Content-Length"))
        {
            const char* s = value.c_str();
            char* endp = NULL;
            long n = std::strtol(s, &endp, 10);
            if (endp != s && n >= 0)
            {
                out_response.setExpectedContentLength(
                    static_cast<unsigned long>(n));
            }
            continue;
        }

        Result<void> h = out_response.appendHeader(name, value);
        if (h.isError())
            return h;
    }

    return Result<void>();
}

}  // namespace http
