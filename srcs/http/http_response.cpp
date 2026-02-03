#include "http/http_response.hpp"

#include <cstdlib>
#include <sstream>

#include "http/header.hpp"

namespace http
{

const std::string HttpResponse::kDefaultHttpVersion = "HTTP/1.1";

HttpResponse::HttpResponse()
    : phase_(kWaitingForHeaders),
      http_version_(kDefaultHttpVersion),
      status_(HttpStatus::OK),
      reason_phrase_(HttpStatus(HttpStatus::OK).getMessage()),
      headers_(),
      has_expected_content_length_(false),
      expected_content_length_(0)
{
}

HttpResponse::HttpResponse(HttpStatus status)
    : phase_(kWaitingForHeaders),
      http_version_(kDefaultHttpVersion),
      status_(status),
      reason_phrase_(status.getMessage()),
      headers_(),
      has_expected_content_length_(false),
      expected_content_length_(0)
{
}

HttpResponse::HttpResponse(const HttpResponse& rhs)
    : phase_(rhs.phase_),
      http_version_(rhs.http_version_),
      status_(rhs.status_),
      reason_phrase_(rhs.reason_phrase_),
      headers_(rhs.headers_),
      has_expected_content_length_(rhs.has_expected_content_length_),
      expected_content_length_(rhs.expected_content_length_)
{
}

HttpResponse& HttpResponse::operator=(const HttpResponse& rhs)
{
    if (this == &rhs)
        return *this;

    phase_ = rhs.phase_;
    http_version_ = rhs.http_version_;
    status_ = rhs.status_;
    reason_phrase_ = rhs.reason_phrase_;
    headers_ = rhs.headers_;
    has_expected_content_length_ = rhs.has_expected_content_length_;
    expected_content_length_ = rhs.expected_content_length_;
    return *this;
}

HttpResponse::~HttpResponse() {}

void HttpResponse::reset()
{
    phase_ = kWaitingForHeaders;
    http_version_ = kDefaultHttpVersion;
    status_ = HttpStatus::OK;
    reason_phrase_ = HttpStatus(HttpStatus::OK).getMessage();
    headers_.clear();
    has_expected_content_length_ = false;
    expected_content_length_ = 0;
}

HttpResponse::Phase HttpResponse::phase() const { return phase_; }

bool HttpResponse::isComplete() const { return phase_ == kComplete; }

bool HttpResponse::hasError() const { return phase_ == kError; }

Result<void> HttpResponse::setStatus(HttpStatus status)
{
    Result<void> ok = ensureHeaderEditable_();
    if (!ok.isOk())
        return ok;
    status_ = status;
    reason_phrase_ = status.getMessage();
    return Result<void>();
}

Result<void> HttpResponse::setStatus(
    HttpStatus status, const std::string& reason_phrase)
{
    Result<void> ok = ensureHeaderEditable_();
    if (!ok.isOk())
        return ok;
    status_ = status;
    reason_phrase_ = reason_phrase;
    return Result<void>();
}

HttpStatus HttpResponse::getStatus() const { return status_; }

const std::string& HttpResponse::getReasonPhrase() const
{
    return reason_phrase_;
}

Result<void> HttpResponse::setHttpVersion(const std::string& version)
{
    Result<void> ok = ensureHeaderEditable_();
    if (!ok.isOk())
        return ok;
    http_version_ = version;
    return Result<void>();
}

const std::string& HttpResponse::getHttpVersion() const
{
    return http_version_;
}

Result<void> HttpResponse::setHeader(
    const std::string& name, const std::string& value)
{
    Result<void> ok = ensureHeaderEditable_();
    if (!ok.isOk())
        return ok;
    std::vector<std::string>& v = headers_[name];
    v.clear();
    v.push_back(value);
    return refreshExpectedContentLength_();
}

Result<void> HttpResponse::appendHeader(
    const std::string& name, const std::string& value)
{
    Result<void> ok = ensureHeaderEditable_();
    if (!ok.isOk())
        return ok;
    headers_[name].push_back(value);
    return refreshExpectedContentLength_();
}

Result<void> HttpResponse::removeHeader(const std::string& name)
{
    Result<void> ok = ensureHeaderEditable_();
    if (!ok.isOk())
        return ok;
    HeaderMap::iterator it = headers_.find(name);
    if (it != headers_.end())
        headers_.erase(it);
    return refreshExpectedContentLength_();
}

bool HttpResponse::hasHeader(const std::string& name) const
{
    return headers_.find(name) != headers_.end();
}

Result<const std::vector<std::string>&> HttpResponse::getHeader(
    const std::string& name) const
{
    HeaderMap::const_iterator it = headers_.find(name);
    if (it == headers_.end())
        return Result<const std::vector<std::string>&>(
            ERROR, "header not found");
    return it->second;
}

const HeaderMap& HttpResponse::getHeaders() const { return headers_; }

bool HttpResponse::hasExpectedContentLength() const
{
    return has_expected_content_length_;
}

unsigned long HttpResponse::expectedContentLength() const
{
    return expected_content_length_;
}

Result<void> HttpResponse::setExpectedContentLength(unsigned long n)
{
    Result<void> ok = ensureHeaderEditable_();
    if (!ok.isOk())
        return ok;
    has_expected_content_length_ = true;
    expected_content_length_ = n;

    std::ostringstream oss;
    oss << n;
    std::vector<std::string>& v =
        headers_[HeaderName(HeaderName::CONTENT_LENGTH).toString()];
    v.clear();
    v.push_back(oss.str());
    return Result<void>();
}

Result<void> HttpResponse::markHeadersFlushed()
{
    if (phase_ == kError)
        return Result<void>(ERROR, "response is in error state");
    if (phase_ == kComplete)
        return Result<void>(ERROR, "response is already complete");
    if (phase_ == kHeadersFlushed)
        return Result<void>();
    if (phase_ != kWaitingForHeaders)
        return Result<void>(ERROR, "invalid phase transition");

    phase_ = kHeadersFlushed;
    return Result<void>();
}

Result<void> HttpResponse::markComplete()
{
    if (phase_ == kError)
        return Result<void>(ERROR, "response is in error state");
    if (phase_ == kComplete)
        return Result<void>();
    if (phase_ != kHeadersFlushed && phase_ != kWaitingForHeaders)
        return Result<void>(ERROR, "invalid phase transition");

    phase_ = kComplete;
    return Result<void>();
}

Result<void> HttpResponse::ensureHeaderEditable_() const
{
    if (phase_ == kWaitingForHeaders)
        return Result<void>();
    if (phase_ == kHeadersFlushed)
        return Result<void>(ERROR, "headers already flushed");
    if (phase_ == kComplete)
        return Result<void>(ERROR, "response already complete");
    return Result<void>(ERROR, "response is in error state");
}

Result<void> HttpResponse::refreshExpectedContentLength_()
{
    HeaderMap::const_iterator it =
        headers_.find(HeaderName(HeaderName::CONTENT_LENGTH).toString());
    if (it == headers_.end() || it->second.empty())
    {
        has_expected_content_length_ = false;
        expected_content_length_ = 0;
        return Result<void>();
    }

    const std::string& s = it->second[0];
    if (s.empty())
    {
        has_expected_content_length_ = false;
        expected_content_length_ = 0;
        return Result<void>();
    }

    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] < '0' || s[i] > '9')
        {
            has_expected_content_length_ = false;
            expected_content_length_ = 0;
            return Result<void>(ERROR, "invalid Content-Length");
        }
    }

    char* end = NULL;
    const unsigned long n = std::strtoul(s.c_str(), &end, 10);
    if (end == NULL || *end != '\0')
    {
        has_expected_content_length_ = false;
        expected_content_length_ = 0;
        return Result<void>(ERROR, "invalid Content-Length");
    }

    has_expected_content_length_ = true;
    expected_content_length_ = n;
    return Result<void>();
}

}  // namespace http
