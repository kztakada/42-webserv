#include <gtest/gtest.h>

#include "http/cgi_meta_variables.hpp"
#include "http/http_request.hpp"

static std::vector<utils::Byte> toBytes_(const char* s)
{
    std::vector<utils::Byte> out;
    if (s == NULL)
        return out;
    for (const char* p = s; *p != '\0'; ++p)
        out.push_back(static_cast<utils::Byte>(*p));
    return out;
}

TEST(CgiMetaVariables, MergesMultipleCookieHeadersWithSemicolonSpace)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes_(
        "GET /cgi/cookie.py HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Cookie: a=1\r\n"
        "Cookie: b=2\r\n"
        "\r\n");

    utils::result::Result<size_t> r = req.parse(buf);
    ASSERT_TRUE(r.isOk());
    ASSERT_TRUE(req.isParseComplete());
    ASSERT_FALSE(req.hasParseError());

    http::CgiMetaVariables meta = http::CgiMetaVariables::fromHttpRequest(
        req, "/cgi/cookie.py", "/cgi/cookie.py");
    const std::map<std::string, std::string>& env = meta.getAll();

    std::map<std::string, std::string>::const_iterator it =
        env.find("HTTP_COOKIE");
    ASSERT_TRUE(it != env.end());
    EXPECT_EQ(std::string("a=1; b=2"), it->second);
}

TEST(CgiMetaVariables, MergesNonCookieRepeatedHeadersWithComma)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes_(
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "X-Test: 1\r\n"
        "X-Test: 2\r\n"
        "\r\n");

    utils::result::Result<size_t> r = req.parse(buf);
    ASSERT_TRUE(r.isOk());
    ASSERT_TRUE(req.isParseComplete());
    ASSERT_FALSE(req.hasParseError());

    http::CgiMetaVariables meta =
        http::CgiMetaVariables::fromHttpRequest(req, "/", "/");
    const std::map<std::string, std::string>& env = meta.getAll();

    std::map<std::string, std::string>::const_iterator it =
        env.find("HTTP_X_TEST");
    ASSERT_TRUE(it != env.end());
    EXPECT_EQ(std::string("1,2"), it->second);
}
