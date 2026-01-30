#include "server/request_router/location_routing.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/server_config.hpp"
#include "server/config/virtual_server_conf.hpp"

static server::VirtualServer makeVirtualServerWithSingleLocation(
    const server::LocationDirectiveConf& loc_conf, const std::string& root_dir,
    const std::string& port)
{
    server::VirtualServerConf vs;
    EXPECT_TRUE(vs.appendListen("", port).isOk());
    EXPECT_TRUE(vs.setRootDir(root_dir).isOk());
    EXPECT_TRUE(vs.appendLocation(loc_conf).isOk());
    EXPECT_TRUE(vs.isValid());
    return server::VirtualServer(vs);
}

TEST(LocationRouting, GetPathRejectsWhenStatusIsError)
{
    server::LocationRouting r(
        NULL, NULL, "/", "", 1, http::HttpStatus::BAD_REQUEST);
    utils::result::Result<std::string> p = r.getPath();
    EXPECT_TRUE(p.isError());
}

TEST(LocationRouting, GetPathRejectsWhenNoLocationMatched)
{
    server::LocationRouting r(NULL, NULL, "/", "", 1, http::HttpStatus::OK);
    utils::result::Result<std::string> p = r.getPath();
    EXPECT_TRUE(p.isError());
}

TEST(LocationRouting, GetPathResolvesWithLocation)
{
    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir("/var/www").isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, "/var/www", "8080");
    const server::LocationDirective* matched =
        vserver.findLocationByPath("/index.html");
    ASSERT_TRUE(matched != NULL);

    server::LocationRouting r(
        &vserver, matched, "/index.html", "", 1, http::HttpStatus::OK);
    utils::result::Result<std::string> p = r.getPath();
    ASSERT_TRUE(p.isOk());
    EXPECT_EQ(p.unwrap(), std::string("/var/www/index.html"));
}

static std::string makeTempDirOrDie_()
{
    char tmpl[] = "/tmp/webserv_location_routing_XXXXXX";
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

TEST(LocationRouting, ResolvePhysicalPathUnderRootOrErrorAllowsNormalPath)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/dir");
    writeFileOrDie_(root + "/dir/file.txt", "ok");

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir(root).isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, root, "8080");
    const server::LocationDirective* matched =
        vserver.findLocationByPath("/dir/file.txt");
    ASSERT_TRUE(matched != NULL);

    server::LocationRouting r(
        &vserver, matched, "/dir/file.txt", "", 1, http::HttpStatus::OK);

    utils::result::Result<utils::path::PhysicalPath> p =
        r.resolvePhysicalPathUnderRootOrError();
    ASSERT_TRUE(p.isOk());
    EXPECT_EQ(p.unwrap().str(), root + "/dir/file.txt");
}

TEST(LocationRouting, ResolvePhysicalPathUnderRootOrErrorRejectsSymlinkEscape)
{
    const std::string base = makeTempDirOrDie_();
    const std::string root = base + "/root";
    const std::string outside = base + "/outside";
    mkdirOrDie_(root);
    mkdirOrDie_(outside);
    writeFileOrDie_(outside + "/secret.txt", "secret");

    int rc = ::symlink("../outside", (root + "/link").c_str());
    ASSERT_EQ(rc, 0);

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir(root).isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, root, "8080");
    const server::LocationDirective* matched = vserver.findLocationByPath("/");
    ASSERT_TRUE(matched != NULL);

    server::LocationRouting r(
        &vserver, matched, "/link/secret.txt", "", 1, http::HttpStatus::OK);

    utils::result::Result<utils::path::PhysicalPath> p =
        r.resolvePhysicalPathUnderRootOrError();
    EXPECT_TRUE(p.isError());
}

TEST(LocationRouting, GetErrorPagePathAllowsExternalUrlButRejectsInternal)
{
    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir("/var/www").isOk());
    ASSERT_TRUE(
        loc.appendErrorPage(http::HttpStatus::NOT_FOUND, "http://e/404.html")
            .isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, "/var/www", "8080");
    const server::LocationDirective* matched = vserver.findLocationByPath("/");
    ASSERT_TRUE(matched != NULL);

    server::LocationRouting r(
        &vserver, matched, "/", "example.com", 1, http::HttpStatus::OK);

    utils::result::Result<std::string> ext =
        r.getErrorPagePath(http::HttpStatus::NOT_FOUND);
    ASSERT_TRUE(ext.isOk());
    EXPECT_EQ(ext.unwrap(), std::string("http://e/404.html"));

    server::LocationDirectiveConf loc2;
    ASSERT_TRUE(loc2.setPathPattern("/").isOk());
    ASSERT_TRUE(loc2.setRootDir("/var/www").isOk());
    ASSERT_TRUE(
        loc2.appendErrorPage(http::HttpStatus::NOT_FOUND, "/errors/404.html")
            .isOk());

    server::VirtualServer vserver2 =
        makeVirtualServerWithSingleLocation(loc2, "/var/www", "8080");
    const server::LocationDirective* matched2 =
        vserver2.findLocationByPath("/");
    ASSERT_TRUE(matched2 != NULL);

    server::LocationRouting r2(
        &vserver2, matched2, "/", "example.com", 1, http::HttpStatus::OK);
    utils::result::Result<std::string> internal =
        r2.getErrorPagePath(http::HttpStatus::NOT_FOUND);
    EXPECT_TRUE(internal.isError());
}

TEST(LocationRouting, GetErrorRequestBuildsInternalRedirectRequest)
{
    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir("/var/www").isOk());
    ASSERT_TRUE(
        loc.appendErrorPage(http::HttpStatus::NOT_FOUND, "/errors/404.html")
            .isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, "/var/www", "8080");
    const server::LocationDirective* matched = vserver.findLocationByPath("/");
    ASSERT_TRUE(matched != NULL);

    server::LocationRouting r(
        &vserver, matched, "/", "example.com", 1, http::HttpStatus::OK);

    utils::result::Result<http::HttpRequest> err_req =
        r.getErrorRequest(http::HttpStatus::NOT_FOUND);
    ASSERT_TRUE(err_req.isOk());

    http::HttpRequest req = err_req.unwrap();
    EXPECT_EQ(req.getMethod(), http::HttpMethod::GET);
    EXPECT_EQ(req.getPath(), std::string("/errors/404.html"));
    EXPECT_EQ(req.getMinorVersion(), 1);

    utils::result::Result<const std::vector<std::string>&> host =
        req.getHeader("Host");
    ASSERT_TRUE(host.isOk());
    ASSERT_FALSE(host.unwrap().empty());
    EXPECT_EQ(host.unwrap()[0], std::string("example.com"));
}

TEST(LocationRouting, DefaultErrorPageContainsStatusLine)
{
    std::string body = server::LocationRouting::getDefaultErrorPage(
        http::HttpStatus::NOT_FOUND);
    EXPECT_NE(body.find("404"), std::string::npos);
    EXPECT_NE(body.find("Not Found"), std::string::npos);
}
