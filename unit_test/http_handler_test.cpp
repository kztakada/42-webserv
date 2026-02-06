#include <gtest/gtest.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/location_directive_conf.hpp"
#include "server/config/server_config.hpp"
#include "server/config/virtual_server_conf.hpp"
#include "server/http_processing_module/request_router/request_router.hpp"
#include "server/session/fd_session/http_session/http_request_handler.hpp"

using utils::result::Result;

static server::ServerConfig makeServerConfig(unsigned long client_max_body_size)
{
    server::ServerConfig config;

    server::VirtualServerConf s;
    EXPECT_TRUE(s.appendListen("", "8080").isOk());
    EXPECT_TRUE(s.setRootDir("/var/www").isOk());
    EXPECT_TRUE(s.appendServerName("example.com").isOk());

    server::LocationDirectiveConf loc;
    EXPECT_TRUE(loc.setPathPattern("/").isOk());
    EXPECT_TRUE(loc.setClientMaxBodySize(client_max_body_size).isOk());
    EXPECT_TRUE(s.appendLocation(loc).isOk());

    EXPECT_TRUE(config.appendServer(s).isOk());
    EXPECT_TRUE(config.isValid());
    return config;
}

static void mustReadAll(int fd, std::string* out)
{
    out->clear();
    char buf[256];
    while (true)
    {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        ASSERT_GE(n, 0);
        if (n == 0)
            break;
        out->append(buf, static_cast<size_t>(n));
    }
}

TEST(HttpRequestHandler, AppliesUnlimitedMaxBodyBytesFromLocation)
{
    server::ServerConfig config = makeServerConfig(0);
    server::RequestRouter router(config);

    Result<IPAddress> ip = IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    http::HttpRequest request;
    http::HttpRequest::Limits limits = request.getLimits();
    limits.max_body_bytes = 5;  // デフォルトが小さい想定
    request.setLimits(limits);

    server::HttpRequestHandler handler(
        request, router, ip.unwrap(), port.unwrap(), &request);

    server::IoBuffer recv;
    const std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "0123456789";
    recv.append(raw);

    Result<void> r = handler.consumeFromRecvBuffer(recv);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    EXPECT_TRUE(request.isParseComplete());
    EXPECT_EQ(request.getDecodedBodyBytes(), static_cast<size_t>(10));

    Result<int> fd = handler.bodyStore().openForRead();
    ASSERT_TRUE(fd.isOk()) << fd.getErrorMessage();
    std::string stored;
    mustReadAll(fd.unwrap(), &stored);
    ::close(fd.unwrap());
    EXPECT_EQ(stored, std::string("0123456789"));
}

TEST(HttpRequestHandler, RejectsBodyOverLocationMaxBodyBytes)
{
    server::ServerConfig config = makeServerConfig(3);
    server::RequestRouter router(config);

    Result<IPAddress> ip = IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    http::HttpRequest request;
    http::HttpRequest::Limits limits = request.getLimits();
    limits.max_body_bytes = 100;
    request.setLimits(limits);

    server::HttpRequestHandler handler(
        request, router, ip.unwrap(), port.unwrap(), &request);

    server::IoBuffer recv;
    const std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "ABCD";
    recv.append(raw);

    Result<void> r = handler.consumeFromRecvBuffer(recv);
    ASSERT_TRUE(r.isError());

    EXPECT_TRUE(request.hasParseError());
    EXPECT_EQ(
        request.getParseErrorStatus(), http::HttpStatus::PAYLOAD_TOO_LARGE);
}
