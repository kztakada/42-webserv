#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/server_config.hpp"
#include "server/config/virtual_server_conf.hpp"
#include "server/http_processing_module/request_processor.hpp"
#include "utils/data_type.hpp"

static std::vector<utils::Byte> toBytes_(const std::string& s)
{
    std::vector<utils::Byte> out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
        out.push_back(static_cast<utils::Byte>(s[i]));
    return out;
}

static http::HttpRequest mustParseRequest_(const std::string& raw)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes_(raw);
    utils::result::Result<size_t> parsed = req.parse(buf);
    EXPECT_TRUE(parsed.isOk());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    return req;
}

static std::string makeTempDirOrDie_()
{
    char tmpl[] = "/tmp/webserv_request_processor_XXXXXX";
    char* d = ::mkdtemp(tmpl);
    EXPECT_TRUE(d != NULL);
    return std::string(d ? d : "");
}

static void mkdirOrDie_(const std::string& p)
{
    int rc = ::mkdir(p.c_str(), 0700);
    EXPECT_EQ(rc, 0);
}

static void writeFileOrDie_(const std::string& p, const std::string& content)
{
    FILE* f = ::fopen(p.c_str(), "wb");
    ASSERT_TRUE(f != NULL);
    ::fwrite(content.data(), 1, content.size(), f);
    ::fclose(f);
}

static void chmodOrDie_(const std::string& p, mode_t mode)
{
    int rc = ::chmod(p.c_str(), mode);
    EXPECT_EQ(rc, 0);
}

static std::string readAll_(server::BodySource* bs)
{
    if (bs == NULL)
        return std::string();
    std::string out;
    for (;;)
    {
        utils::result::Result<server::BodySource::ReadResult> r =
            bs->read(1024);
        if (r.isError())
            return std::string();
        const server::BodySource::ReadResult rr = r.unwrap();
        for (size_t i = 0; i < rr.data.size(); ++i)
            out.push_back(static_cast<char>(rr.data[i]));
        if (rr.status == server::BodySource::READ_EOF)
            break;
        if (rr.status == server::BodySource::READ_WOULD_BLOCK)
            continue;
    }
    return out;
}

TEST(RequestProcessor, NotFoundAppliesErrorPageAndPreserves404)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/errors");
    writeFileOrDie_(root + "/errors/404.html", "Custom404");

    server::VirtualServerConf vs;
    ASSERT_TRUE(vs.appendListen("", "8080").isOk());
    ASSERT_TRUE(vs.setRootDir(root).isOk());
    ASSERT_TRUE(
        vs.appendErrorPage(http::HttpStatus::NOT_FOUND, "/errors/404.html")
            .isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(vs.appendLocation(loc).isOk());
    ASSERT_TRUE(vs.isValid());

    server::ServerConfig cfg;
    ASSERT_TRUE(cfg.appendServer(vs).isOk());
    ASSERT_TRUE(cfg.isValid());

    server::RequestRouter router(cfg);

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    server::RequestProcessor proc(router);

    http::HttpRequest req = mustParseRequest_(
        "GET /missing.txt HTTP/1.1\r\nHost: example.com\r\n\r\n");

    http::HttpResponse resp;
    utils::result::Result<server::RequestProcessor::Output> out =
        proc.process(req, ip.unwrap(), port.unwrap(), resp);
    ASSERT_TRUE(out.isOk());

    EXPECT_EQ(resp.getStatus().getCode(), 404u);
    server::RequestProcessor::Output o = out.unwrap();
    ASSERT_TRUE(o.body_source.get() != NULL);

    std::string body = readAll_(o.body_source.get());
    EXPECT_EQ(body, std::string("Custom404"));

    (void)root;
}

TEST(RequestProcessor, UnreadableIndexReturns403Not404EvenWith404ErrorPage)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/errors");
    writeFileOrDie_(root + "/errors/404.html", "Custom404");
    writeFileOrDie_(root + "/index.html", "INDEX");
    chmodOrDie_(root + "/index.html", 0000);

    server::VirtualServerConf vs;
    ASSERT_TRUE(vs.appendListen("", "8080").isOk());
    ASSERT_TRUE(vs.setRootDir(root).isOk());
    ASSERT_TRUE(
        vs.appendErrorPage(http::HttpStatus::NOT_FOUND, "/errors/404.html")
            .isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(vs.appendLocation(loc).isOk());
    ASSERT_TRUE(vs.isValid());

    server::ServerConfig cfg;
    ASSERT_TRUE(cfg.appendServer(vs).isOk());
    ASSERT_TRUE(cfg.isValid());

    server::RequestRouter router(cfg);

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    server::RequestProcessor proc(router);

    http::HttpRequest req =
        mustParseRequest_("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");

    http::HttpResponse resp;
    utils::result::Result<server::RequestProcessor::Output> out =
        proc.process(req, ip.unwrap(), port.unwrap(), resp);
    ASSERT_TRUE(out.isOk());

    EXPECT_EQ(resp.getStatus().getCode(), 403u);

    server::RequestProcessor::Output o = out.unwrap();
    ASSERT_TRUE(o.body_source.get() != NULL);
    std::string body = readAll_(o.body_source.get());
    EXPECT_NE(body, std::string("Custom404"));

    (void)root;
}

TEST(RequestProcessor, UnreadableFileReturns403Not404EvenWith404ErrorPage)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/errors");
    writeFileOrDie_(root + "/errors/404.html", "Custom404");
    writeFileOrDie_(root + "/hello.txt", "HELLO");
    chmodOrDie_(root + "/hello.txt", 0000);

    server::VirtualServerConf vs;
    ASSERT_TRUE(vs.appendListen("", "8080").isOk());
    ASSERT_TRUE(vs.setRootDir(root).isOk());
    ASSERT_TRUE(
        vs.appendErrorPage(http::HttpStatus::NOT_FOUND, "/errors/404.html")
            .isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(vs.appendLocation(loc).isOk());
    ASSERT_TRUE(vs.isValid());

    server::ServerConfig cfg;
    ASSERT_TRUE(cfg.appendServer(vs).isOk());
    ASSERT_TRUE(cfg.isValid());

    server::RequestRouter router(cfg);

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    server::RequestProcessor proc(router);

    http::HttpRequest req = mustParseRequest_(
        "GET /hello.txt HTTP/1.1\r\nHost: example.com\r\n\r\n");

    http::HttpResponse resp;
    utils::result::Result<server::RequestProcessor::Output> out =
        proc.process(req, ip.unwrap(), port.unwrap(), resp);
    ASSERT_TRUE(out.isOk());

    EXPECT_EQ(resp.getStatus().getCode(), 403u);

    server::RequestProcessor::Output o = out.unwrap();
    ASSERT_TRUE(o.body_source.get() != NULL);
    std::string body = readAll_(o.body_source.get());
    EXPECT_NE(body, std::string("Custom404"));

    (void)root;
}

TEST(RequestProcessor, ForbiddenAppliesErrorPageAndPreserves403)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/errors");
    mkdirOrDie_(root + "/dir");
    writeFileOrDie_(root + "/errors/403.html", "Custom403");

    server::VirtualServerConf vs;
    ASSERT_TRUE(vs.appendListen("", "8080").isOk());
    ASSERT_TRUE(vs.setRootDir(root).isOk());
    ASSERT_TRUE(
        vs.appendErrorPage(http::HttpStatus::FORBIDDEN, "/errors/403.html")
            .isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(vs.appendLocation(loc).isOk());
    ASSERT_TRUE(vs.isValid());

    server::ServerConfig cfg;
    ASSERT_TRUE(cfg.appendServer(vs).isOk());
    ASSERT_TRUE(cfg.isValid());

    server::RequestRouter router(cfg);

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    server::RequestProcessor proc(router);

    // /dir/ はディレクトリで、index/autoindex が無いので 403 になる。
    http::HttpRequest req =
        mustParseRequest_("GET /dir/ HTTP/1.1\r\nHost: example.com\r\n\r\n");

    http::HttpResponse resp;
    utils::result::Result<server::RequestProcessor::Output> out =
        proc.process(req, ip.unwrap(), port.unwrap(), resp);
    ASSERT_TRUE(out.isOk());

    EXPECT_EQ(resp.getStatus().getCode(), 403u);
    server::RequestProcessor::Output o = out.unwrap();
    ASSERT_TRUE(o.body_source.get() != NULL);

    std::string body = readAll_(o.body_source.get());
    EXPECT_EQ(body, std::string("Custom403"));

    (void)root;
}

TEST(RequestProcessor, ProcessErrorAppliesErrorPageAndPreserves500)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/errors");
    writeFileOrDie_(root + "/errors/500.html", "Custom500");

    server::VirtualServerConf vs;
    ASSERT_TRUE(vs.appendListen("", "8080").isOk());
    ASSERT_TRUE(vs.setRootDir(root).isOk());
    ASSERT_TRUE(
        vs.appendErrorPage(http::HttpStatus::SERVER_ERROR, "/errors/500.html")
            .isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(vs.appendLocation(loc).isOk());
    ASSERT_TRUE(vs.isValid());

    server::ServerConfig cfg;
    ASSERT_TRUE(cfg.appendServer(vs).isOk());
    ASSERT_TRUE(cfg.isValid());

    server::RequestRouter router(cfg);

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    server::RequestProcessor proc(router);

    http::HttpRequest req =
        mustParseRequest_("GET /any HTTP/1.1\r\nHost: example.com\r\n\r\n");

    http::HttpResponse resp;
    utils::result::Result<server::RequestProcessor::Output> out =
        proc.processError(req, ip.unwrap(), port.unwrap(),
            http::HttpStatus::SERVER_ERROR, resp);
    ASSERT_TRUE(out.isOk());

    EXPECT_EQ(resp.getStatus().getCode(), 500u);
    server::RequestProcessor::Output o = out.unwrap();
    ASSERT_TRUE(o.body_source.get() != NULL);

    std::string body = readAll_(o.body_source.get());
    EXPECT_EQ(body, std::string("Custom500"));

    (void)root;
}

TEST(RequestProcessor, UnsupportedMethodReturns405AndAppliesErrorPage)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/errors");
    writeFileOrDie_(root + "/errors/405.html", "Custom405");

    server::VirtualServerConf vs;
    ASSERT_TRUE(vs.appendListen("", "8080").isOk());
    ASSERT_TRUE(vs.setRootDir(root).isOk());
    ASSERT_TRUE(
        vs.appendErrorPage(http::HttpStatus::NOT_ALLOWED, "/errors/405.html")
            .isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(vs.appendLocation(loc).isOk());
    ASSERT_TRUE(vs.isValid());

    server::ServerConfig cfg;
    ASSERT_TRUE(cfg.appendServer(vs).isOk());
    ASSERT_TRUE(cfg.isValid());

    server::RequestRouter router(cfg);

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    server::RequestProcessor proc(router);

    const std::string methods[] = {"PUT", "HEAD", "OPTIONS", "PATCH", "TRACE"};

    for (size_t i = 0; i < (sizeof(methods) / sizeof(methods[0])); ++i)
    {
        // 既知のメソッドだけパース自体は成功する。
        http::HttpRequest req;
        std::string raw =
            methods[i] + std::string(
                             " /any HTTP/1.1\r\nHost: "
                             "example.com\r\nContent-Length: 0\r\n\r\n");
        std::vector<utils::Byte> buf = toBytes_(raw);
        utils::result::Result<size_t> parsed = req.parse(buf);
        ASSERT_TRUE(parsed.isOk());
        ASSERT_TRUE(req.isParseComplete());
        ASSERT_FALSE(req.hasParseError());

        // 通常フローは process()。
        http::HttpResponse resp;
        utils::result::Result<server::RequestProcessor::Output> out =
            proc.process(req, ip.unwrap(), port.unwrap(), resp);
        ASSERT_TRUE(out.isOk());

        EXPECT_EQ(resp.getStatus().getCode(), 405u);
        ASSERT_TRUE(resp.hasHeader("Allow"));
        utils::result::Result<const std::vector<std::string>&> allow =
            resp.getHeader("Allow");
        ASSERT_TRUE(allow.isOk());
        ASSERT_FALSE(allow.unwrap().empty());
        EXPECT_NE(allow.unwrap()[0].find("GET"), std::string::npos);

        server::RequestProcessor::Output o = out.unwrap();
        ASSERT_TRUE(o.body_source.get() != NULL);

        std::string body = readAll_(o.body_source.get());
        EXPECT_EQ(body, std::string("Custom405"));
    }

    (void)root;
}

TEST(RequestProcessor, NotFoundUsesDefaultErrorPageWhenNoCustom)
{
    const std::string root = makeTempDirOrDie_();

    server::VirtualServerConf vs;
    ASSERT_TRUE(vs.appendListen("", "8080").isOk());
    ASSERT_TRUE(vs.setRootDir(root).isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(vs.appendLocation(loc).isOk());
    ASSERT_TRUE(vs.isValid());

    server::ServerConfig cfg;
    ASSERT_TRUE(cfg.appendServer(vs).isOk());
    ASSERT_TRUE(cfg.isValid());

    server::RequestRouter router(cfg);

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    server::RequestProcessor proc(router);

    http::HttpRequest req = mustParseRequest_(
        "GET /missing.txt HTTP/1.1\r\nHost: example.com\r\n\r\n");

    http::HttpResponse resp;
    utils::result::Result<server::RequestProcessor::Output> out =
        proc.process(req, ip.unwrap(), port.unwrap(), resp);
    ASSERT_TRUE(out.isOk());

    EXPECT_EQ(resp.getStatus().getCode(), 404u);
    server::RequestProcessor::Output o = out.unwrap();
    ASSERT_TRUE(o.body_source.get() != NULL);

    std::string body = readAll_(o.body_source.get());
    EXPECT_NE(body.find("404"), std::string::npos);
    EXPECT_NE(body.find("Not Found"), std::string::npos);

    (void)root;
}
