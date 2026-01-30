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
    EXPECT_TRUE(conf.setListenPort("8080").isOk());
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
    EXPECT_TRUE(conf.setListenPort("8080").isOk());
    EXPECT_TRUE(conf.setRootDir("/var/www").isOk());

    server::LocationDirectiveConf loc;
    loc.redirect_url = "/new";
    loc.redirect_status = http::HttpStatus::OK;
    conf.locations.push_back(loc);

    EXPECT_FALSE(conf.isValid());
}

TEST(VirtualServerConf, DuplicateSingleValueDirectivesReturnError)
{
    server::VirtualServerConf conf;

    EXPECT_TRUE(conf.setListenPort("8080").isOk());
    EXPECT_TRUE(conf.setListenPort("8081").isError());
    EXPECT_TRUE(conf.setListenIp("127.0.0.1").isError());

    EXPECT_TRUE(conf.setRootDir("/var/www").isOk());
    EXPECT_TRUE(conf.setRootDir("/var/www2").isError());

    EXPECT_TRUE(conf.setClientMaxBodySize(123).isOk());
    EXPECT_TRUE(conf.setClientMaxBodySize(456).isError());
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

    EXPECT_TRUE(conf.setListenPort("8080").isOk());
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

    EXPECT_TRUE(v.setListenPort("8080").isOk());
    EXPECT_TRUE(v.setRootDir("/var/www").isOk());
    EXPECT_TRUE(v.appendLocation(server::LocationDirectiveConf()).isOk());
    EXPECT_TRUE(conf.appendServer(v).isOk());
    EXPECT_TRUE(conf.isValid());
}
