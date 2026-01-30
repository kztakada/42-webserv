#include "server/request_router/resolved_path.hpp"

#include <gtest/gtest.h>

#include "http/http_request.hpp"

static std::vector<utils::Byte> toBytes(const char* s)
{
    std::vector<utils::Byte> out;
    if (s == NULL)
        return out;
    for (const char* p = s; *p != '\0'; ++p)
        out.push_back(static_cast<utils::Byte>(*p));
    return out;
}

static http::HttpRequest mustParseRequest(const char* raw)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes(raw);
    utils::result::Result<size_t> parsed = req.parse(buf);
    EXPECT_TRUE(parsed.isOk());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    return req;
}

TEST(ResolvedPath, NormalizesSlashesAndExtractsHost)
{
    http::HttpRequest req = mustParseRequest(
        "GET /a//b///c HTTP/1.1\r\nHost: example.com:8080\r\n\r\n");

    utils::result::Result<server::ResolvedPath> resolved =
        server::ResolvedPath::create(req);
    ASSERT_TRUE(resolved.isOk());

    EXPECT_EQ(resolved.unwrap().getPath(), std::string("/a/b/c"));
    EXPECT_EQ(resolved.unwrap().getHost(), std::string("example.com"));
    EXPECT_EQ(resolved.unwrap().getMinorVersion(), 1);
}

TEST(ResolvedPath, ResolveDotSegmentsRemovesDotAndParent)
{
    http::HttpRequest req =
        mustParseRequest("GET /a/b/./c/../d/ HTTP/1.1\r\nHost: x\r\n\r\n");

    utils::result::Result<server::ResolvedPath> resolved =
        server::ResolvedPath::create(req);
    ASSERT_TRUE(resolved.isOk());

    utils::result::Result<std::string> out =
        resolved.unwrap().resolveDotSegmentsOrError();
    ASSERT_TRUE(out.isOk());
    EXPECT_EQ(out.unwrap(), std::string("/a/b/d/"));
}

TEST(ResolvedPath, ResolveDotSegmentsRejectsEscapingRoot)
{
    http::HttpRequest req =
        mustParseRequest("GET /../a HTTP/1.1\r\nHost: x\r\n\r\n");

    utils::result::Result<server::ResolvedPath> resolved =
        server::ResolvedPath::create(req);
    ASSERT_TRUE(resolved.isOk());

    utils::result::Result<std::string> out =
        resolved.unwrap().resolveDotSegmentsOrError();
    EXPECT_TRUE(out.isError());
}
