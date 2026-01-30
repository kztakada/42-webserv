#include <gtest/gtest.h>

#include <climits>

#include "http/http_method.hpp"
#include "server/config/server_config.hpp"
#include "utils/path.hpp"
#include "utils/result.hpp"

TEST(LocationDirectiveConf, DefaultsAreValid)
{
    server::LocationDirectiveConf conf;
    EXPECT_TRUE(conf.isValid());
    EXPECT_EQ(1024ul * 1024ul, conf.client_max_body_size);
    EXPECT_FALSE(conf.auto_index);
    EXPECT_TRUE(conf.redirect_url.empty());
    EXPECT_EQ(http::HttpStatus::UNKNOWN, conf.redirect_status);
    EXPECT_TRUE(conf.cgi_extensions.empty());
}

TEST(LocationDirectiveConf, InvalidWhenBodySizeExceedsIntMax)
{
    server::LocationDirectiveConf conf;
    utils::result::Result<void> r =
        conf.setClientMaxBodySize(static_cast<unsigned long>(INT_MAX) + 1ul);
    EXPECT_TRUE(r.isError());
    EXPECT_TRUE(conf.isValid());
    EXPECT_EQ(1024ul * 1024ul, conf.client_max_body_size);
}

TEST(LocationDirectiveConf, CgiExtensionsMustHaveNonEmptyKeysAndValues)
{
    server::LocationDirectiveConf conf;
    utils::result::Result<void> r =
        conf.appendCgiExtension(".py", "/usr/bin/python3");
    EXPECT_TRUE(r.isOk());
    EXPECT_TRUE(conf.isValid());
    EXPECT_EQ("/usr/bin/python3", conf.cgi_extensions[".py"].str());

    r = conf.appendCgiExtension("py", "/usr/bin/python3");
    EXPECT_TRUE(r.isError());
    EXPECT_TRUE(conf.isValid());

    r = conf.appendCgiExtension("", "/bin/sh");
    EXPECT_TRUE(r.isError());
    EXPECT_TRUE(conf.isValid());
}

TEST(LocationDirectiveConf, PhysicalPathsAreResolvedAndNormalized)
{
    server::LocationDirectiveConf conf;
    utils::result::Result<std::string> cwd =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(cwd.isOk());

    EXPECT_TRUE(conf.setRootDir("./www//html").isOk());
    EXPECT_EQ(cwd.unwrap() + "/www/html", conf.root_dir.str());

    EXPECT_TRUE(conf.setUploadStore("www/uploads/../uploads/tmp").isOk());
    EXPECT_EQ(cwd.unwrap() + "/www/uploads/tmp", conf.upload_store.str());

    EXPECT_TRUE(
        conf.appendCgiExtension(".py", "./tester//ubuntu_cgi_tester").isOk());
    EXPECT_EQ(cwd.unwrap() + "/tester/ubuntu_cgi_tester",
        conf.cgi_extensions[".py"].str());
}

TEST(LocationDirectiveConf, ErrorPageMustBeAbsoluteUriPathOrHttpUrl)
{
    server::LocationDirectiveConf conf;

    EXPECT_TRUE(
        conf.appendErrorPage(http::HttpStatus::NOT_FOUND, "/errors//404.html")
            .isOk());
    EXPECT_EQ(
        "/errors//404.html", conf.error_pages[http::HttpStatus::NOT_FOUND]);

    EXPECT_TRUE(
        conf.appendErrorPage(
                http::HttpStatus::SERVER_ERROR, "http://example.com/err")
            .isOk());
    EXPECT_EQ("http://example.com/err",
        conf.error_pages[http::HttpStatus::SERVER_ERROR]);

    EXPECT_TRUE(
        conf.appendErrorPage(http::HttpStatus::BAD_REQUEST, "./err.html")
            .isError());
    EXPECT_TRUE(conf.appendErrorPage(http::HttpStatus::BAD_REQUEST, "err.html")
            .isError());
}

TEST(LocationDirectiveConf, RedirectUrlMustBeAbsolutePathOrHttpUrl)
{
    {
        server::LocationDirectiveConf conf;
        EXPECT_TRUE(
            conf.setRedirect(http::HttpStatus::MOVED_PERMANENTLY, "/new")
                .isOk());
    }
    {
        server::LocationDirectiveConf conf;
        EXPECT_TRUE(
            conf.setRedirect(http::HttpStatus::FOUND, "http://example.com/new")
                .isOk());
    }
    {
        server::LocationDirectiveConf conf;
        EXPECT_TRUE(
            conf.setRedirect(http::HttpStatus::FOUND, "https://example.com/new")
                .isOk());
    }
    {
        server::LocationDirectiveConf conf;
        EXPECT_TRUE(conf.setRedirect(http::HttpStatus::FOUND, "new-page.html")
                .isError());
    }
}

TEST(LocationDirectiveConf, RejectsNulInPathTokens)
{
    server::LocationDirectiveConf conf;

    std::string with_nul = std::string("a\0b", 3);

    EXPECT_TRUE(conf.setRootDir(with_nul).isError());
    EXPECT_TRUE(conf.appendIndexPage(with_nul).isError());
    EXPECT_TRUE(
        conf.appendErrorPage(http::HttpStatus::NOT_FOUND, with_nul).isError());
    EXPECT_TRUE(conf.setUploadStore(with_nul).isError());
}

TEST(LocationDirectiveConf, RedirectStatusMustBeRedirectionWhenUrlIsSet)
{
    server::LocationDirectiveConf conf;
    utils::result::Result<void> r =
        conf.setRedirect(http::HttpStatus::OK, "/new-page");
    EXPECT_TRUE(r.isError());
    EXPECT_TRUE(conf.isValid());

    r = conf.setRedirect(http::HttpStatus::MOVED_PERMANENTLY, "/new-page");
    EXPECT_TRUE(r.isOk());
    EXPECT_TRUE(conf.isValid());
}

TEST(LocationDirectiveConf, DuplicateSingleValueDirectivesReturnError)
{
    server::LocationDirectiveConf conf;

    EXPECT_TRUE(conf.setRootDir("/var/www").isOk());
    EXPECT_TRUE(conf.setRootDir("/var/www2").isError());

    EXPECT_TRUE(conf.setClientMaxBodySize(123).isOk());
    EXPECT_TRUE(conf.setClientMaxBodySize(456).isError());

    EXPECT_TRUE(conf.setAutoIndex(true).isOk());
    EXPECT_TRUE(conf.setAutoIndex(false).isError());

    EXPECT_TRUE(
        conf.setRedirect(http::HttpStatus::MOVED_PERMANENTLY, "/new").isOk());
    EXPECT_TRUE(conf.setRedirect(http::HttpStatus::FOUND, "/new2").isError());

    EXPECT_TRUE(conf.setUploadStore("/tmp").isOk());
    EXPECT_TRUE(conf.setUploadStore("/tmp2").isError());
}

TEST(LocationDirectiveConf, AllowMethodsDuplicatesBecomeUnique)
{
    server::LocationDirectiveConf conf;

    EXPECT_TRUE(conf.appendAllowedMethod(http::HttpMethod::GET).isOk());
    EXPECT_TRUE(conf.appendAllowedMethod(http::HttpMethod::GET).isOk());
    EXPECT_TRUE(conf.appendAllowedMethod(http::HttpMethod::POST).isOk());

    EXPECT_TRUE(conf.has_allowed_methods);
    EXPECT_EQ(2u, conf.allowed_methods.size());
    EXPECT_EQ(1u, conf.allowed_methods.count(http::HttpMethod::GET));
    EXPECT_EQ(1u, conf.allowed_methods.count(http::HttpMethod::POST));
    EXPECT_TRUE(conf.isValid());
}

TEST(VirtualServerConf, DefaultsAreInvalidBecauseListenAndRootMissing)
{
    server::VirtualServerConf conf;
    EXPECT_TRUE(conf.index_pages.size() >= 1);
    EXPECT_FALSE(conf.isValid());
}

TEST(VirtualServerConf, BecomesValidWithListenPortRootAndValidLocations)
{
    server::VirtualServerConf conf;
    EXPECT_TRUE(conf.appendListen("", "8080").isOk());
    utils::result::Result<std::string> cwd =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(cwd.isOk());

    EXPECT_TRUE(conf.setRootDir("./www//html").isOk());
    EXPECT_EQ(cwd.unwrap() + "/www/html", conf.root_dir.str());

    server::LocationDirectiveConf loc;
    EXPECT_TRUE(conf.appendLocation(loc).isOk());

    EXPECT_TRUE(conf.isValid());
}

TEST(VirtualServerConf, InvalidIfAnyLocationInvalid)
{
    server::VirtualServerConf conf;
    EXPECT_TRUE(conf.appendListen("", "8080").isOk());
    EXPECT_TRUE(conf.setRootDir("/var/www").isOk());

    server::LocationDirectiveConf loc;
    loc.redirect_url = "/new";
    loc.redirect_status = http::HttpStatus::OK;
    conf.locations.push_back(loc);

    EXPECT_FALSE(conf.isValid());
}

TEST(VirtualServerConf, ListensAllowMultipleButRejectExactDuplicates)
{
    server::VirtualServerConf conf;

    EXPECT_TRUE(conf.appendListen("", "8080").isOk());
    EXPECT_TRUE(conf.appendListen("", "8081").isOk());
    EXPECT_TRUE(conf.appendListen("127.0.0.1", "8080").isOk());
    EXPECT_TRUE(conf.appendListen("", "8080").isError());

    EXPECT_TRUE(conf.setRootDir("/var/www").isOk());
    EXPECT_TRUE(conf.setRootDir("/var/www2").isError());

    EXPECT_TRUE(conf.setClientMaxBodySize(123).isOk());
    EXPECT_TRUE(conf.setClientMaxBodySize(456).isError());
}

TEST(VirtualServerConf, GetListensResolvesWildcardAndDuplicates)
{
    server::VirtualServerConf conf;

    // 同一 port で wildcard が存在する場合、他IPは除外される
    EXPECT_TRUE(conf.appendListen("127.0.0.1", "8080").isOk());
    EXPECT_TRUE(conf.appendListen("192.168.0.10", "8080").isOk());
    EXPECT_TRUE(conf.appendListen("", "8080").isOk());  // ipv4Any (0.0.0.0)
    EXPECT_TRUE(
        conf.appendListen("0.0.0.0", "8080").isError());  // exact duplicate

    // wildcard が無い port は完全同一 (IP:port) のみ 1つに畳み込む
    EXPECT_TRUE(conf.appendListen("127.0.0.1", "8081").isOk());
    EXPECT_TRUE(conf.appendListen("127.0.0.1", "8081").isError());
    EXPECT_TRUE(conf.appendListen("192.168.0.10", "8081").isOk());

    std::vector<server::Listen> listens = conf.getListens();

    bool has_wild_8080 = false;
    bool has_127_8080 = false;
    bool has_192_8080 = false;
    bool has_127_8081 = false;
    bool has_192_8081 = false;

    for (size_t i = 0; i < listens.size(); ++i)
    {
        const std::string ip = listens[i].host_ip.toString();
        const std::string port = listens[i].port.toString();
        if (port == "8080" && ip == "0.0.0.0")
            has_wild_8080 = true;
        if (port == "8080" && ip == "127.0.0.1")
            has_127_8080 = true;
        if (port == "8080" && ip == "192.168.0.10")
            has_192_8080 = true;
        if (port == "8081" && ip == "127.0.0.1")
            has_127_8081 = true;
        if (port == "8081" && ip == "192.168.0.10")
            has_192_8081 = true;
    }

    EXPECT_TRUE(has_wild_8080);
    EXPECT_FALSE(has_127_8080);
    EXPECT_FALSE(has_192_8080);
    EXPECT_TRUE(has_127_8081);
    EXPECT_TRUE(has_192_8081);
}

TEST(VirtualServerConf, ErrorPageMustBeAbsoluteUriPathOrHttpUrl)
{
    server::VirtualServerConf conf;
    EXPECT_TRUE(
        conf.appendErrorPage(http::HttpStatus::NOT_FOUND, "/404.html").isOk());
    EXPECT_TRUE(
        conf.appendErrorPage(
                http::HttpStatus::SERVER_ERROR, "https://example.com/err")
            .isOk());
    EXPECT_TRUE(conf.appendErrorPage(http::HttpStatus::BAD_REQUEST, "err.html")
            .isError());
}

TEST(VirtualServerConf, RejectsNulInPathTokens)
{
    server::VirtualServerConf conf;
    std::string with_nul = std::string("a\0b", 3);

    EXPECT_TRUE(conf.appendListen("", "8080").isOk());
    EXPECT_TRUE(conf.setRootDir(with_nul).isError());
    EXPECT_TRUE(conf.appendIndexPage(with_nul).isError());
    EXPECT_TRUE(
        conf.appendErrorPage(http::HttpStatus::NOT_FOUND, with_nul).isError());
}

TEST(ServerConfig, IsValidRequiresAtLeastOneValidServer)
{
    server::ServerConfig conf;
    EXPECT_FALSE(conf.isValid());

    server::VirtualServerConf v;
    EXPECT_TRUE(conf.appendServer(v).isError());
    EXPECT_FALSE(conf.isValid());

    EXPECT_TRUE(v.appendListen("", "8080").isOk());
    EXPECT_TRUE(v.setRootDir("/var/www").isOk());
    EXPECT_TRUE(v.appendLocation(server::LocationDirectiveConf()).isOk());
    EXPECT_TRUE(conf.appendServer(v).isOk());
    EXPECT_TRUE(conf.isValid());
}
