#include "http/syntax.hpp"

namespace http
{

const std::string HttpSyntax::kCrlf = "\r\n";
const std::string HttpSyntax::kHeaderBoundary =
    HttpSyntax::kCrlf + HttpSyntax::kCrlf;
const std::string HttpSyntax::kHttpVersionPrefix = "HTTP/";
const std::string HttpSyntax::kExpectMajorVersion = "1.";
const int HttpSyntax::kMinorVersionDigitLimit = 3;
const std::string HttpSyntax::kOWS = "\t ";  // Optional White Space
const std::string HttpSyntax::kTcharsWithoutAlnum = "!#$%&'*+-.^_`|~";
const int HttpSyntax::kMaxUriLength = 2000;

}  // namespace http
