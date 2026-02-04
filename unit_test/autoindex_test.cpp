#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/server_config.hpp"
#include "server/config/virtual_server_conf.hpp"
#include "server/request_processor/request_processor.hpp"
#include "utils/byte.hpp"

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
    char tmpl[] = "/tmp/webserv_autoindex_XXXXXX";
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

static std::string readAll_(server::BodySource* bs)
{
    if (bs == NULL)
        return std::string();
    std::string out;
    for (;;)
    {
        utils::result::Result<server::BodySource::ReadResult> r =
            bs->read(4096);
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

TEST(RequestProcessor, AutoIndexReturnsDirectoryListingHtml)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/dir");
    mkdirOrDie_(root + "/dir/sub");
    writeFileOrDie_(root + "/dir/a.txt", "A");

    server::VirtualServerConf vs;
    ASSERT_TRUE(vs.appendListen("", "8080").isOk());
    ASSERT_TRUE(vs.setRootDir(root).isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setAutoIndex(true).isOk());
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

    server::RequestProcessor proc(router, ip.unwrap(), port.unwrap());

    http::HttpRequest req =
        mustParseRequest_("GET /dir/ HTTP/1.1\r\nHost: example.com\r\n\r\n");

    http::HttpResponse resp;
    utils::result::Result<server::RequestProcessor::Output> out =
        proc.process(req, resp);
    ASSERT_TRUE(out.isOk());

    EXPECT_EQ(resp.getStatus().getCode(), 200u);
    ASSERT_TRUE(out.unwrap().body_source != NULL);

    std::string body = readAll_(out.unwrap().body_source);
    EXPECT_NE(body.find("Index of /dir/"), std::string::npos);
    EXPECT_NE(body.find("a.txt"), std::string::npos);
    EXPECT_NE(body.find("sub/"), std::string::npos);

    delete out.unwrap().body_source;
}
