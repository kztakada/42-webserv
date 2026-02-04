#include "server/request_router/location_routing.hpp"

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/server_config.hpp"
#include "server/config/virtual_server_conf.hpp"
#include "utils/byte.hpp"

static std::vector<utils::Byte> toBytes_(const char* s)
{
    std::vector<utils::Byte> out;
    if (s == NULL)
        return out;
    for (const char* p = s; *p != '\0'; ++p)
        out.push_back(static_cast<utils::Byte>(*p));
    return out;
}

static http::HttpRequest mustParseRequest_(const std::string& raw)
{
    http::HttpRequest req;
    std::vector<utils::Byte> buf = toBytes_(raw.c_str());
    utils::result::Result<size_t> parsed = req.parse(buf);
    EXPECT_TRUE(parsed.isOk());
    EXPECT_TRUE(req.isParseComplete());
    EXPECT_FALSE(req.hasParseError());
    return req;
}

static http::HttpRequest makeRequest_(const std::string& method,
    const std::string& target, const std::string& host, int minor_version,
    const std::string& content_length)
{
    std::ostringstream oss;
    oss << method << " " << target << " HTTP/1." << minor_version << "\r\n";
    if (!host.empty())
    {
        oss << "Host: " << host << "\r\n";
    }
    if (!content_length.empty())
    {
        oss << "Content-Length: " << content_length << "\r\n";
    }
    oss << "\r\n";
    return mustParseRequest_(oss.str());
}

static http::HttpRequest makeGet_(
    const std::string& target, const std::string& host, int minor_version)
{
    return makeRequest_("GET", target, host, minor_version, "");
}

static http::HttpRequest makePost_(const std::string& target,
    const std::string& host, int minor_version,
    const std::string& content_length)
{
    return makeRequest_("POST", target, host, minor_version, content_length);
}

static http::HttpRequest makePut_(const std::string& target,
    const std::string& host, int minor_version,
    const std::string& content_length)
{
    return makeRequest_("PUT", target, host, minor_version, content_length);
}

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
    http::HttpRequest req = makeGet_("/", "", 0);
    server::ResolvedRequestContext ctx =
        server::ResolvedRequestContext::createForBadRequest(req);
    server::LocationRouting r(
        NULL, NULL, ctx, req, http::HttpStatus::BAD_REQUEST);
    EXPECT_EQ(r.getNextAction(), server::RESPOND_ERROR);
    EXPECT_EQ(r.getHttpStatus().getCode(), 400u);
}

TEST(LocationRouting, NoLocationMatchedBecomes404RespondError)
{
    http::HttpRequest req = makeGet_("/", "", 0);
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        NULL, NULL, ctx_res.unwrap(), req, http::HttpStatus::OK);
    EXPECT_EQ(r.getNextAction(), server::RESPOND_ERROR);
    EXPECT_EQ(r.getHttpStatus().getCode(), 404u);
}

TEST(LocationRouting, NoLocationMatchedAppliesServerErrorPage404)
{
    server::VirtualServerConf vs;
    ASSERT_TRUE(vs.appendListen("", "8080").isOk());
    ASSERT_TRUE(vs.setRootDir("/var/www").isOk());
    ASSERT_TRUE(
        vs.appendErrorPage(http::HttpStatus::NOT_FOUND, "/errors/404.html")
            .isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/a").isOk());
    ASSERT_TRUE(vs.appendLocation(loc).isOk());
    ASSERT_TRUE(vs.isValid());

    server::VirtualServer vserver(vs);
    const server::LocationDirective* matched = vserver.findLocationByPath("/b");
    ASSERT_TRUE(matched == NULL);

    http::HttpRequest req = makeGet_("/b", "", 0);
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());

    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

    EXPECT_EQ(r.getHttpStatus().getCode(), 404u);
    EXPECT_EQ(r.getNextAction(), server::REDIRECT_INTERNAL);
    utils::result::Result<std::string> loc_hdr = r.getRedirectLocation();
    ASSERT_TRUE(loc_hdr.isOk());
    EXPECT_EQ(loc_hdr.unwrap(), std::string("/errors/404.html"));
}

TEST(LocationRouting, ReturnRedirectExternalIsPrioritized)
{
    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir("/var/www").isOk());
    ASSERT_TRUE(
        loc.setRedirect(http::HttpStatus::MOVED_PERMANENTLY, "http://e/new")
            .isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, "/var/www", "8080");
    const server::LocationDirective* matched = vserver.findLocationByPath("/");
    ASSERT_TRUE(matched != NULL);

    http::HttpRequest req = makeGet_("/", "example.com", 1);
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

    EXPECT_EQ(r.getNextAction(), server::REDIRECT_EXTERNAL);
    EXPECT_EQ(r.getHttpStatus().getCode(), 301u);
    utils::result::Result<std::string> loc_hdr = r.getRedirectLocation();
    ASSERT_TRUE(loc_hdr.isOk());
    EXPECT_EQ(loc_hdr.unwrap(), std::string("http://e/new"));
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

    http::HttpRequest req = makeGet_("/dir/file.txt", "", 0);
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

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

    http::HttpRequest req = makeGet_("/link/secret.txt", "", 0);
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

    utils::result::Result<utils::path::PhysicalPath> p =
        r.resolvePhysicalPathUnderRootOrError();
    EXPECT_TRUE(p.isError());
}

TEST(LocationRouting, ReturnRedirectInternalBuildsInternalRequest)
{
    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir("/var/www").isOk());
    ASSERT_TRUE(loc.setRedirect(http::HttpStatus::FOUND, "/moved").isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, "/var/www", "8080");
    const server::LocationDirective* matched = vserver.findLocationByPath("/");
    ASSERT_TRUE(matched != NULL);

    http::HttpRequest req = makeGet_("/", "example.com", 1);
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

    EXPECT_EQ(r.getNextAction(), server::REDIRECT_INTERNAL);
    utils::result::Result<std::string> loc_hdr = r.getRedirectLocation();
    ASSERT_TRUE(loc_hdr.isOk());
    EXPECT_EQ(loc_hdr.unwrap(), std::string("/moved"));

    utils::result::Result<http::HttpRequest> next =
        r.getInternalRedirectRequest();
    ASSERT_TRUE(next.isOk());
    EXPECT_EQ(next.unwrap().getPath(), std::string("/moved"));
    EXPECT_EQ(next.unwrap().getMinorVersion(), 1);
}

TEST(LocationRouting, PayloadTooLargeByContentLength)
{
    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir("/var/www").isOk());
    ASSERT_TRUE(loc.setClientMaxBodySize(3).isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, "/var/www", "8080");
    const server::LocationDirective* matched = vserver.findLocationByPath("/");
    ASSERT_TRUE(matched != NULL);

    std::string body(10, 'x');
    std::ostringstream oss;
    oss << "POST / HTTP/1.1\r\n";
    oss << "Host: example.com\r\n";
    oss << "Content-Length: 10\r\n";
    oss << "\r\n";
    oss << body;
    http::HttpRequest req = mustParseRequest_(oss.str());
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

    EXPECT_EQ(r.getNextAction(), server::RESPOND_ERROR);
    EXPECT_EQ(r.getHttpStatus().getCode(), 413u);
}

TEST(LocationRouting, MethodNotAllowedBecomes405)
{
    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir("/var/www").isOk());
    ASSERT_TRUE(loc.appendAllowedMethod(http::HttpMethod::GET).isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, "/var/www", "8080");
    const server::LocationDirective* matched = vserver.findLocationByPath("/");
    ASSERT_TRUE(matched != NULL);

    http::HttpRequest req = makePost_("/", "example.com", 1, "0");
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

    EXPECT_EQ(r.getNextAction(), server::RESPOND_ERROR);
    EXPECT_EQ(r.getHttpStatus().getCode(), 405u);
}

TEST(LocationRouting, UploadStoreSelectsStoreBodyAndReturnsContext)
{
    const std::string root = makeTempDirOrDie_();
    const std::string store = makeTempDirOrDie_();

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/upload").isOk());
    ASSERT_TRUE(loc.setRootDir(root).isOk());
    ASSERT_TRUE(loc.setUploadStore(store).isOk());
    ASSERT_TRUE(loc.appendAllowedMethod(http::HttpMethod::POST).isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, root, "8080");
    const server::LocationDirective* matched =
        vserver.findLocationByPath("/upload/file.txt");
    ASSERT_TRUE(matched != NULL);

    http::HttpRequest req =
        makePost_("/upload/file.txt", "example.com", 1, "0");
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

    EXPECT_EQ(r.getNextAction(), server::STORE_BODY);
    utils::result::Result<server::UploadContext> up = r.getUploadContext();
    ASSERT_TRUE(up.isOk());
    EXPECT_EQ(up.unwrap().store_root.str(), store);
    EXPECT_EQ(up.unwrap().destination_path.str(), store + "/file.txt");
    EXPECT_TRUE(up.unwrap().allow_create_leaf);
    EXPECT_FALSE(up.unwrap().allow_overwrite);
}

TEST(LocationRouting, AutoIndexContextContainsDirectoryPhysicalPath)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/d");

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir(root).isOk());
    ASSERT_TRUE(loc.setAutoIndex(true).isOk());
    // VirtualServerConf のデフォルト index.html を無効化して
    // 「index候補なし + autoindex有効」を作る。
    loc.has_index_pages = true;
    loc.index_pages.clear();

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, root, "8080");
    const server::LocationDirective* matched =
        vserver.findLocationByPath("/d/");
    ASSERT_TRUE(matched != NULL);

    http::HttpRequest req = makeGet_("/d/", "example.com", 1);
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

    EXPECT_EQ(r.getNextAction(), server::SERVE_AUTOINDEX);
    utils::result::Result<server::AutoIndexContext> a = r.getAutoIndexContext();
    ASSERT_TRUE(a.isOk());
    EXPECT_EQ(a.unwrap().uri_dir_path, std::string("/d/"));
    EXPECT_EQ(a.unwrap().directory_path.str(), root + "/d");
    EXPECT_TRUE(a.unwrap().autoindex_enabled);
    EXPECT_TRUE(a.unwrap().index_candidates.empty());
}

TEST(LocationRouting, AutoIndexContextRejectsNonDirectoryUri)
{
    const std::string root = makeTempDirOrDie_();
    writeFileOrDie_(root + "/file.txt", "x");

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(loc.setRootDir(root).isOk());
    ASSERT_TRUE(loc.setAutoIndex(true).isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, root, "8080");
    const server::LocationDirective* matched =
        vserver.findLocationByPath("/file.txt");
    ASSERT_TRUE(matched != NULL);

    http::HttpRequest req = makeGet_("/file.txt", "example.com", 1);
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);
    EXPECT_EQ(r.getNextAction(), server::SERVE_STATIC);
    EXPECT_TRUE(r.getAutoIndexContext().isError());
}

TEST(LocationRouting, CgiContextSplitsScriptNameAndPathInfo)
{
    const std::string root = makeTempDirOrDie_();
    mkdirOrDie_(root + "/cgi-bin");

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/cgi-bin").isOk());
    ASSERT_TRUE(loc.setRootDir(root).isOk());
    ASSERT_TRUE(loc.appendCgiExtension(".cgi", "/bin/sh").isOk());

    server::VirtualServer vserver =
        makeVirtualServerWithSingleLocation(loc, root, "8080");
    const server::LocationDirective* matched =
        vserver.findLocationByPath("/cgi-bin/test.cgi/hoge");
    ASSERT_TRUE(matched != NULL);

    http::HttpRequest req =
        makeGet_("/cgi-bin/test.cgi/hoge?a=b", "example.com", 1);
    utils::result::Result<server::ResolvedRequestContext> ctx_res =
        server::ResolvedRequestContext::create(req);
    ASSERT_TRUE(ctx_res.isOk());
    server::LocationRouting r(
        &vserver, matched, ctx_res.unwrap(), req, http::HttpStatus::OK);

    EXPECT_EQ(r.getNextAction(), server::RUN_CGI);
    utils::result::Result<server::CgiContext> c = r.getCgiContext();
    ASSERT_TRUE(c.isOk());
    EXPECT_EQ(c.unwrap().script_name, std::string("/cgi-bin/test.cgi"));
    EXPECT_EQ(c.unwrap().path_info, std::string("/hoge"));
    EXPECT_EQ(c.unwrap().query_string, std::string("a=b"));
    EXPECT_EQ(c.unwrap().script_filename.str(), root + "/test.cgi");
}
