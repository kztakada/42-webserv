#include "server/request_router/request_router.hpp"

#include <gtest/gtest.h>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/location_directive_conf.hpp"
#include "server/config/server_config.hpp"
#include "server/config/virtual_server_conf.hpp"

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

static server::ServerConfig makeTwoServerConfig()
{
    server::ServerConfig config;

    server::VirtualServerConf a;
    EXPECT_TRUE(a.appendListen("", "8080").isOk());
    EXPECT_TRUE(a.setRootDir("/var/www_a").isOk());
    EXPECT_TRUE(a.appendServerName("a.example.com").isOk());

    server::LocationDirectiveConf a_loc;
    EXPECT_TRUE(a_loc.setPathPattern("/a").isOk());
    EXPECT_TRUE(a.appendLocation(a_loc).isOk());

    EXPECT_TRUE(config.appendServer(a).isOk());

    server::VirtualServerConf b;
    EXPECT_TRUE(b.appendListen("", "8080").isOk());
    EXPECT_TRUE(b.setRootDir("/var/www_b").isOk());
    EXPECT_TRUE(b.appendServerName("b.example.com").isOk());

    server::LocationDirectiveConf b_loc;
    EXPECT_TRUE(b_loc.setPathPattern("/b").isOk());
    EXPECT_TRUE(b.appendLocation(b_loc).isOk());

    EXPECT_TRUE(config.appendServer(b).isOk());

    EXPECT_TRUE(config.isValid());
    return config;
}

TEST(RequestRouter, SelectsVirtualServerByHostAndNormalizesPath)
{
    server::ServerConfig config = makeTwoServerConfig();
    server::RequestRouter router(config);

    http::HttpRequest req = mustParseRequest(
        "GET /b//x/../y HTTP/1.1\r\nHost: b.example.com\r\n\r\n");

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    utils::result::Result<server::LocationRouting> routed =
        router.route(req, ip.unwrap(), port.unwrap());
    ASSERT_TRUE(routed.isOk());

    EXPECT_EQ(routed.unwrap().getHttpStatus(), http::HttpStatus::OK);
    EXPECT_EQ(routed.unwrap().getNextAction(), server::SERVE_STATIC);

    utils::result::Result<std::string> uri = routed.unwrap().getStaticUriPath();
    ASSERT_TRUE(uri.isOk());
    EXPECT_EQ(uri.unwrap(), std::string("/b/y"));
}

TEST(RequestRouter, DefaultsToFirstMatchingServerWhenHostNotMatched)
{
    server::ServerConfig config = makeTwoServerConfig();
    server::RequestRouter router(config);

    http::HttpRequest req = mustParseRequest(
        "GET /a/test HTTP/1.1\r\nHost: unknown.example.com\r\n\r\n");

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    utils::result::Result<server::LocationRouting> routed =
        router.route(req, ip.unwrap(), port.unwrap());
    ASSERT_TRUE(routed.isOk());

    EXPECT_EQ(routed.unwrap().getHttpStatus(), http::HttpStatus::OK);
    EXPECT_EQ(routed.unwrap().getNextAction(), server::SERVE_STATIC);

    utils::result::Result<std::string> uri = routed.unwrap().getStaticUriPath();
    ASSERT_TRUE(uri.isOk());
    EXPECT_EQ(uri.unwrap(), std::string("/a/test"));
}

TEST(RequestRouter, ReturnsBadRequestWhenPathEscapesRoot)
{
    server::ServerConfig config = makeTwoServerConfig();
    server::RequestRouter router(config);

    http::HttpRequest req = mustParseRequest(
        "GET /../secret HTTP/1.1\r\nHost: a.example.com\r\n\r\n");

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    utils::result::Result<server::LocationRouting> routed =
        router.route(req, ip.unwrap(), port.unwrap());
    ASSERT_TRUE(routed.isOk());

    EXPECT_EQ(routed.unwrap().getHttpStatus(), http::HttpStatus::BAD_REQUEST);
    EXPECT_EQ(routed.unwrap().getNextAction(), server::RESPOND_ERROR);
}
