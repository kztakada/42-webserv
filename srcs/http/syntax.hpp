#ifndef HTTP_CONSTANTS_HPP_
#define HTTP_CONSTANTS_HPP_

#include <string>

namespace http
{

// RFC 7230で定義されたHTTPプロトコル定数
struct HttpSyntax
{
    // Message Format (RFC 7230 Section 3)
    static const std::string kCrlf;
    static const std::string kHeaderBoundary;
    static const std::string kHttpVersionPrefix;
    static const std::string kExpectMajorVersion;

    // Limits and Constraints
    static const int kMinorVersionDigitLimit;
    static const int kMaxUriLength;

    // Syntax Elements (RFC 7230 Section 3.2)
    static const std::string kOWS;  // Optional White Space
    static const std::string kTcharsWithoutAlnum;

   private:
    HttpSyntax() {}  // インスタンス化を防止
};

// 静的メンバーの定義（ヘッダーオンリー実装）
const std::string HttpSyntax::kCrlf = "\r\n";
const std::string HttpSyntax::kHeaderBoundary = kCrlf + kCrlf;
const std::string HttpSyntax::kHttpVersionPrefix = "HTTP/";
const std::string HttpSyntax::kExpectMajorVersion = "1.";
const int HttpSyntax::kMinorVersionDigitLimit = 3;
const std::string HttpSyntax::kOWS = "\t ";  // Optional White Space
const std::string HttpSyntax::kTcharsWithoutAlnum = "!#$%&'*+-.^_`|~";
const int HttpSyntax::kMaxUriLength = 2000;

}  // namespace http

#endif