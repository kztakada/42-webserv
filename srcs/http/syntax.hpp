#ifndef HTTP_CONSTANTS_HPP_
#define HTTP_CONSTANTS_HPP_

#include <string>

namespace http
{

// RFC 9112で定義されたHTTPプロトコル定数
struct HttpSyntax
{
    // Message Format (RFC 9112 Section 2)
    static const std::string kCrlf;
    static const std::string kHeaderBoundary;
    static const std::string kHttpVersionPrefix;
    static const std::string kExpectMajorVersion;

    // Limits and Constraints
    static const int kMinorVersionDigitLimit;
    static const int kMaxUriLength;

    // Syntax Elements (RFC 9110 Section 5.6.2)
    static const std::string kOWS;  // Optional White Space
    static const std::string kTcharsWithoutAlnum;

   private:
    HttpSyntax() {}  // インスタンス化を防止
};

}  // namespace http

#endif
