#include "server/config/parser/config_parser.hpp"

#include <gtest/gtest.h>

#include <fstream>

#include "utils/path.hpp"

TEST(ConfigParser, ParseMinimalServerFromData)
{
    const std::string conf =
        "server {\n"
        "  listen 8080;\n"
        "  root ./www//html;\n"
        "  location / { }\n"
        "}\n";

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseData(conf);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(1u, cfg.servers.size());

    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(1u, s.listens.size());
    EXPECT_EQ("8080", s.listens[0].port.toString());

    utils::result::Result<std::string> cwd =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(cwd.isOk());
    EXPECT_EQ(cwd.unwrap() + "/www/html", s.root_dir.str());

    ASSERT_EQ(1u, s.locations.size());
    EXPECT_EQ("/", s.locations[0].path_pattern);
}

TEST(ConfigParser, SupportsComments)
{
    const std::string conf =
        "# comment\n"
        "server {\n"
        "  listen 127.0.0.1:8080; # inline comment\n"
        "  root ./www/html;\n"
        "}\n";

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseData(conf);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();
    ASSERT_EQ(1u, r.unwrap().servers.size());
}

TEST(ConfigParser, DuplicateServerNameOnSamePortIsError)
{
    const std::string conf =
        "server { listen 8080; server_name example.com; root /var/www; }\n"
        "server { listen 8080; server_name example.com; root /var/www2; }\n";

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseData(conf);
    EXPECT_TRUE(r.isError());
}

TEST(ConfigParser, SameServerNameOnDifferentPortIsOk)
{
    const std::string conf =
        "server { listen 8080; server_name example.com; root /var/www; }\n"
        "server { listen 8081; server_name example.com; root /var/www2; }\n";

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseData(conf);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();
    ASSERT_EQ(2u, r.unwrap().servers.size());
}

TEST(ConfigParser, ParsesClientMaxBodySizeWithSuffix)
{
    const std::string conf =
        "server {\n"
        "  listen 8080;\n"
        "  root /var/www;\n"
        "  client_max_body_size 10M;\n"
        "}\n";

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseData(conf);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    server::ServerConfig cfg = r.unwrap();
    const server::VirtualServerConf& s = cfg.servers[0];
    EXPECT_TRUE(s.has_client_max_body_size);
    EXPECT_EQ(10ul * 1024ul * 1024ul, s.client_max_body_size);
}

TEST(ConfigParser, ParseLocationDirectives)
{
    const std::string conf =
        "server {\n"
        "  listen 8080;\n"
        "  root /var/www;\n"
        "  location /cgi-bin {\n"
        "    allow_methods GET POST;\n"
        "    autoindex on;\n"
        "    upload_store ./www/uploads/../uploads/tmp;\n"
        "    return 301 /new-page.html;\n"
        "    cgi_extension .py ./tester//ubuntu_cgi_tester;\n"
        "  }\n"
        "}\n";

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseData(conf);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    server::ServerConfig cfg = r.unwrap();
    const server::LocationDirectiveConf& loc = cfg.servers[0].locations[0];
    EXPECT_EQ("/cgi-bin", loc.path_pattern);
    EXPECT_TRUE(loc.has_allowed_methods);
    EXPECT_EQ(2u, loc.allowed_methods.size());
    EXPECT_TRUE(loc.has_auto_index);
    EXPECT_TRUE(loc.auto_index);
    EXPECT_FALSE(loc.upload_store.empty());
    EXPECT_EQ(http::HttpStatus::MOVED_PERMANENTLY, loc.redirect_status);
    EXPECT_EQ("/new-page.html", loc.redirect_url);

    utils::result::Result<std::string> cwd =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(cwd.isOk());
    ASSERT_EQ(1u, loc.cgi_extensions.size());
    EXPECT_EQ(cwd.unwrap() + "/tester/ubuntu_cgi_tester",
        loc.cgi_extensions.find(".py")->second.str());
}

TEST(ConfigParser, ParseFile)
{
    const std::string conf = "server { listen 8080; root /var/www; }\n";

    const std::string path = "/tmp/webserv_config_parser_test.conf";
    std::ofstream ofs(path.c_str());
    ASSERT_TRUE(ofs.good());
    ofs << conf;
    ofs.close();

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();
    ASSERT_EQ(1u, r.unwrap().servers.size());
}
