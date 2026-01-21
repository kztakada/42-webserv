#include "http/http_request.hpp"

#include <gtest/gtest.h>

static std::vector<utils::Byte> toBytes(const char* s)
{
    std::vector<utils::Byte> out;
    if (s == NULL)
        return out;
    for (const char* p = s; *p != '\0'; ++p)
        out.push_back(static_cast<utils::Byte>(*p));
    return out;
}

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
    EXPECT_EQ(0ul, req.getBodySize());
}

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
