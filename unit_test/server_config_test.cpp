#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "server/config/parser/config_parser.hpp"
#include "utils/path.hpp"
#include "utils/result.hpp"

namespace
{

static std::string buildConfigPath(const std::string& filename)
{
    return std::string("unit_test/config/success/") + filename;
}

static std::string buildErrorConfigPath(const std::string& filename)
{
    return std::string("unit_test/config/error/") + filename;
}

static void expectParseErrorConfigFile(const std::string& filename)
{
    const std::string path = buildErrorConfigPath(filename);
    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    EXPECT_TRUE(r.isError()) << "unexpectedly parsed: " << path
                             << " (error=" << r.getErrorMessage() << ")";
}

}  // namespace

// 最小構成の設定ファイルがパースでき、ServerConfigがvalidになること
TEST(ServerConfig, ParsesMinimalConfigFileAndBecomesValid)
{
    const std::string path = buildConfigPath("minimal.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    EXPECT_TRUE(cfg.isValid());
    ASSERT_EQ(1u, cfg.servers.size());

    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(1u, s.listens.size());
    EXPECT_EQ("8080", s.listens[0].port.toString());

    utils::result::Result<std::string> cwd =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(cwd.isOk());
    EXPECT_EQ(cwd.unwrap() + "/www/html", s.root_dir.str());

    // デフォルト値（VirtualServerConf::constructor）
    EXPECT_FALSE(s.has_client_max_body_size);
    EXPECT_EQ(1024ul * 1024ul, s.client_max_body_size);
    ASSERT_GE(s.index_pages.size(), 1u);
    EXPECT_EQ("index.html", s.index_pages[0]);
}

// 同一portでも server_name が重複しなければ複数serverを追加できること
TEST(ServerConfig, ParsesMultipleVirtualHostsOnSamePortWithDifferentNames)
{
    const std::string path = buildConfigPath("multi_vhost_same_port.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    EXPECT_TRUE(cfg.isValid());
    ASSERT_EQ(2u, cfg.servers.size());

    EXPECT_EQ(1u, cfg.servers[0].server_names.size());
    EXPECT_EQ(1u, cfg.servers[1].server_names.size());

    EXPECT_TRUE(cfg.servers[0].server_names.count("example.com") == 1u);
    EXPECT_TRUE(cfg.servers[1].server_names.count("other.example.com") == 1u);
}

// 同一portでwildcard(0.0.0.0)が存在する場合、getListens()がwildcard優先で1つに畳まれること
TEST(ServerConfig, GetListensPrefersWildcardOnSamePort)
{
    const std::string path = buildConfigPath("listen_wildcard_precedence.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(1u, cfg.servers.size());

    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(3u, s.listens.size());

    std::vector<server::Listen> listens = s.getListens();

    // 8080については wildcard のみが残る
    ASSERT_EQ(1u, listens.size());
    EXPECT_TRUE(listens[0].host_ip.isWildcard());
    EXPECT_EQ("8080", listens[0].port.toString());
}

// locationディレクティブ（allow_methods/cgi_extension/return/upload_store/autoindex）と
// location back が正常にパースできること
TEST(ServerConfig, ParsesLocationDirectivesAndBackwardLocation)
{
    const std::string path = buildConfigPath("location_features.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(1u, cfg.servers.size());
    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(2u, s.locations.size());

    // /cgi-bin の location
    {
        const server::LocationDirectiveConf& loc = s.locations[0];
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
        server::LocationDirectiveConf::CgiExtensionsMap::const_iterator it =
            loc.cgi_extensions.find(".py");
        ASSERT_TRUE(it != loc.cgi_extensions.end());
        EXPECT_EQ(cwd.unwrap() + "/tester/ubuntu_cgi_tester", it->second.str());
    }

    // location back /suffix
    {
        const server::LocationDirectiveConf& loc = s.locations[1];
        EXPECT_EQ("/suffix", loc.path_pattern);
        EXPECT_TRUE(loc.is_backward_search);
    }
}

// server_name省略/複数server/index/error_page/コメントが混ざっていても正常にパースできること
TEST(ServerConfig, ParsesServerNameOmissionIndexAndErrorPages)
{
    const std::string path =
        buildConfigPath("server_name_and_error_pages.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(2u, cfg.servers.size());

    // 1つ目のserver:
    // server_name未指定、相対rootの解決、index追加、error_pageの保持
    {
        const server::VirtualServerConf& s = cfg.servers[0];
        EXPECT_TRUE(s.server_names.empty());

        utils::result::Result<std::string> cwd =
            utils::path::getCurrentWorkingDirectory();
        ASSERT_TRUE(cwd.isOk());
        EXPECT_EQ(cwd.unwrap() + "/www/html", s.root_dir.str());

        // デフォルトで index.html が入っていて、設定で home.html が追加される
        ASSERT_GE(s.index_pages.size(), 2u);
        EXPECT_EQ("index.html", s.index_pages[0]);
        EXPECT_EQ("home.html", s.index_pages[1]);

        EXPECT_EQ("/404.html",
            s.error_pages.find(http::HttpStatus::NOT_FOUND)->second);
        EXPECT_EQ("http://example.com/err",
            s.error_pages.find(http::HttpStatus::SERVER_ERROR)->second);
    }

    // 2つ目のserver: server_name複数
    {
        const server::VirtualServerConf& s = cfg.servers[1];
        EXPECT_EQ(2u, s.server_names.size());
        EXPECT_TRUE(s.server_names.count("example.com") == 1u);
        EXPECT_TRUE(s.server_names.count("www.example.com") == 1u);
        EXPECT_EQ("8081", s.listens[0].port.toString());
    }
}

// 空のlocationブロック(location / {
// })が正常にパースでき、LocationDirectiveConfがvalidであること
TEST(ServerConfig, ParsesEmptyLocationBlock)
{
    const std::string path = buildConfigPath("empty_location_block.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(1u, s.locations.size());

    const server::LocationDirectiveConf& loc = s.locations[0];
    EXPECT_EQ("/", loc.path_pattern);
    EXPECT_TRUE(loc.isValid());

    // allow_methods未指定: conf側では未セット
    EXPECT_FALSE(loc.has_allowed_methods);
    EXPECT_TRUE(loc.allowed_methods.empty());
}

// client_max_body_size のバイト指定と、location側の上書きが両方パースできること
TEST(ServerConfig, ParsesBodySizeBytesAndLocationOverride)
{
    const std::string path =
        buildConfigPath("size_bytes_and_location_override.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    const server::VirtualServerConf& s = cfg.servers[0];
    EXPECT_TRUE(s.has_client_max_body_size);
    EXPECT_EQ(100ul, s.client_max_body_size);

    ASSERT_EQ(1u, s.locations.size());
    const server::LocationDirectiveConf& loc = s.locations[0];
    EXPECT_TRUE(loc.has_client_max_body_size);
    EXPECT_EQ(200ul, loc.client_max_body_size);
}

// allow_methodsの重複指定が許容され、cgi_extensionを複数定義できること
TEST(ServerConfig, ParsesMethodDuplicatesAndMultipleCgiExtensions)
{
    const std::string path =
        buildConfigPath("method_duplicates_and_multiple_cgi.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(1u, s.locations.size());
    const server::LocationDirectiveConf& loc = s.locations[0];

    EXPECT_TRUE(loc.has_allowed_methods);
    EXPECT_EQ(2u, loc.allowed_methods.size());

    ASSERT_EQ(2u, loc.cgi_extensions.size());
    EXPECT_TRUE(loc.cgi_extensions.find(".py") != loc.cgi_extensions.end());
    EXPECT_TRUE(loc.cgi_extensions.find(".php") != loc.cgi_extensions.end());
}

// wildcard無しの複数listenが getListens() で落ちずに両方残ること
TEST(ServerConfig, GetListensKeepsAllIpsWhenNoWildcard)
{
    const std::string path =
        buildConfigPath("multiple_listens_no_wildcard.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    const server::VirtualServerConf& s = cfg.servers[0];
    std::vector<server::Listen> listens = s.getListens();
    ASSERT_EQ(2u, listens.size());

    bool has_127 = false;
    bool has_192 = false;
    for (size_t i = 0; i < listens.size(); ++i)
    {
        if (listens[i].host_ip.toString() == "127.0.0.1")
        {
            has_127 = true;
        }
        if (listens[i].host_ip.toString() == "192.168.0.10")
        {
            has_192 = true;
        }
        EXPECT_EQ("8080", listens[i].port.toString());
    }
    EXPECT_TRUE(has_127);
    EXPECT_TRUE(has_192);
}

// 意地悪だけど正常: 空白/改行/コメント/トークン詰め込み/"listen
// :8080"/"1m"/returnのみlocation を含んでもパースできること
TEST(ServerConfig, ParsesEvilDenseWhitespaceAndReturnOnlyLocations)
{
    const std::string path = buildConfigPath("evil_dense_whitespace.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(1u, cfg.servers.size());
    const server::VirtualServerConf& s = cfg.servers[0];

    // listen :8080 -> host省略なので wildcard
    ASSERT_EQ(1u, s.listens.size());
    EXPECT_TRUE(s.listens[0].host_ip.isWildcard());
    EXPECT_EQ("8080", s.listens[0].port.toString());

    // root の正規化（./ と .. と // を含む）
    utils::result::Result<std::string> cwd =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(cwd.isOk());
    EXPECT_EQ(cwd.unwrap() + "/www/html", s.root_dir.str());

    // client_max_body_size 1m -> 1MB
    EXPECT_TRUE(s.has_client_max_body_size);
    EXPECT_EQ(1ul * 1024ul * 1024ul, s.client_max_body_size);

    // location: / (empty), /redir(return only), back /suffix(return)
    ASSERT_EQ(3u, s.locations.size());
    EXPECT_EQ("/", s.locations[0].path_pattern);
    EXPECT_EQ("/redir", s.locations[1].path_pattern);
    EXPECT_EQ(http::HttpStatus::FOUND, s.locations[1].redirect_status);
    EXPECT_EQ("https://example.com/new", s.locations[1].redirect_url);
    EXPECT_TRUE(s.locations[2].is_backward_search);
    EXPECT_EQ("/suffix", s.locations[2].path_pattern);
}

// 意地悪だけど正常: error_page同一コードの再定義（後勝ち）と
// index複数引数がパースできること
TEST(ServerConfig, ParsesErrorPageOverrideAndMultipleIndexArguments)
{
    const std::string path =
        buildConfigPath("error_page_override_and_multi_index.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    const server::VirtualServerConf& s = cfg.servers[0];

    // index はデフォルトの index.html + a.html + b.html
    ASSERT_GE(s.index_pages.size(), 3u);
    EXPECT_EQ("index.html", s.index_pages[0]);
    EXPECT_EQ("a.html", s.index_pages[1]);
    EXPECT_EQ("b.html", s.index_pages[2]);

    // error_page 404 は後勝ち
    server::ErrorPagesMap::const_iterator it =
        s.error_pages.find(http::HttpStatus::NOT_FOUND);
    ASSERT_TRUE(it != s.error_pages.end());
    EXPECT_EQ("https://example.com/second404.html", it->second);
}

// 意地悪だけど正常:
// 同じpatternのlocationを複数定義してもパースは成功すること（routing側の解決とは別）
TEST(ServerConfig, ParsesDuplicateLocationPatterns)
{
    const std::string path =
        buildConfigPath("duplicate_location_patterns.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(2u, s.locations.size());

    EXPECT_EQ("/same", s.locations[0].path_pattern);
    EXPECT_FALSE(s.locations[0].is_backward_search);

    EXPECT_EQ("/same", s.locations[1].path_pattern);
    EXPECT_TRUE(s.locations[1].is_backward_search);
}

// 意地悪だけど正常:
// locationを先に書いてrootを後で書いてもパースでき、ServerConfigがvalidになること
TEST(ServerConfig, ParsesOrderIndependentDirectives)
{
    const std::string path =
        buildConfigPath("order_independent_directives.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(1u, cfg.servers.size());
    EXPECT_TRUE(cfg.isValid());

    const server::VirtualServerConf& s = cfg.servers[0];
    EXPECT_FALSE(s.root_dir.empty());
    ASSERT_EQ(1u, s.locations.size());
    EXPECT_EQ("/", s.locations[0].path_pattern);
}

// 意地悪だけど正常: location側の root/index/error_page/autoindex
// がパースされ、フラグも正しく立つこと
TEST(ServerConfig, ParsesLocationIndexAndErrorPage)
{
    const std::string path =
        buildConfigPath("location_index_and_error_page.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(1u, s.locations.size());

    const server::LocationDirectiveConf& loc = s.locations[0];
    EXPECT_EQ("/docs", loc.path_pattern);

    EXPECT_TRUE(loc.has_root_dir);
    EXPECT_EQ("/var/www/docs", loc.root_dir.str());

    EXPECT_TRUE(loc.has_index_pages);
    ASSERT_EQ(2u, loc.index_pages.size());
    EXPECT_EQ("a.html", loc.index_pages[0]);
    EXPECT_EQ("b.html", loc.index_pages[1]);

    EXPECT_TRUE(loc.has_error_pages);
    EXPECT_EQ("/docs404.html",
        loc.error_pages.find(http::HttpStatus::NOT_FOUND)->second);

    EXPECT_TRUE(loc.has_auto_index);
    EXPECT_FALSE(loc.auto_index);
}

// 意地悪だけど正常: 同一server_nameでも listen port
// が違えば複数serverとして許容されること
TEST(ServerConfig, ParsesSameServerNameOnDifferentPorts)
{
    const std::string path =
        buildConfigPath("same_server_name_different_ports.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(2u, cfg.servers.size());

    EXPECT_EQ("8091", cfg.servers[0].listens[0].port.toString());
    EXPECT_EQ("8092", cfg.servers[1].listens[0].port.toString());
    EXPECT_TRUE(cfg.servers[0].server_names.count("www-1.example.com") == 1u);
    EXPECT_TRUE(cfg.servers[1].server_names.count("www-1.example.com") == 1u);
}

// 意地悪だけど正常: listen 0.0.0.0:port
// の明示指定がワイルドカードとしてパースできること
TEST(ServerConfig, ParsesListenExplicitWildcard)
{
    const std::string path = buildConfigPath("listen_explicit_wildcard.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(1u, cfg.servers.size());
    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(1u, s.listens.size());

    EXPECT_TRUE(s.listens[0].host_ip.isWildcard());
    EXPECT_EQ("8093", s.listens[0].port.toString());
}

// 成功パターン: server_name の重複がユニーク化され、
// index ディレクティブを複数回書いても順序通りに追加されること
TEST(ServerConfig, ParsesServerNameDuplicatesAndMultiIndexDirectives)
{
    const std::string path = buildConfigPath(
        "server_name_duplicates_and_multi_index_directives.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(1u, cfg.servers.size());
    const server::VirtualServerConf& s = cfg.servers[0];

    EXPECT_EQ(2u, s.server_names.size());
    EXPECT_TRUE(s.server_names.count("example.com") == 1u);
    EXPECT_TRUE(s.server_names.count("www.example.com") == 1u);

    // index はデフォルトの index.html + a.html + b.html + c.html
    ASSERT_GE(s.index_pages.size(), 4u);
    EXPECT_EQ("index.html", s.index_pages[0]);
    EXPECT_EQ("a.html", s.index_pages[1]);
    EXPECT_EQ("b.html", s.index_pages[2]);
    EXPECT_EQ("c.html", s.index_pages[3]);
}

// 成功パターン: client_max_body_size 0 が許容され、
// root の正規化(// と ..)が解決されること
TEST(ServerConfig, ParsesClientMaxBodySizeZeroAndRootNormalization)
{
    const std::string path = buildConfigPath(
        "client_max_body_size_zero_and_root_normalization.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(1u, cfg.servers.size());
    const server::VirtualServerConf& s = cfg.servers[0];

    EXPECT_TRUE(s.has_client_max_body_size);
    EXPECT_EQ(0ul, s.client_max_body_size);

    utils::result::Result<std::string> cwd =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(cwd.isOk());
    EXPECT_EQ(cwd.unwrap() + "/www/html", s.root_dir.str());
}

// 成功パターン: location 内で
// - upload_store の解決/正規化
// - error_page の後勝ち（外部URLも保持できる）
// - index ディレクティブ複数回
// が成立すること
TEST(ServerConfig, ParsesLocationExternalErrorPageAndUploadStoreNormalization)
{
    const std::string path = buildConfigPath(
        "location_error_page_external_and_upload_store_normalization.conf");

    utils::result::Result<server::ServerConfig> r =
        server::ConfigParser::parseFile(path);
    ASSERT_TRUE(r.isOk()) << r.getErrorMessage();

    const server::ServerConfig cfg = r.unwrap();
    ASSERT_EQ(1u, cfg.servers.size());
    const server::VirtualServerConf& s = cfg.servers[0];
    ASSERT_EQ(1u, s.locations.size());

    const server::LocationDirectiveConf& loc = s.locations[0];
    EXPECT_EQ("/upload", loc.path_pattern);

    utils::result::Result<std::string> cwd =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(cwd.isOk());
    EXPECT_EQ(cwd.unwrap() + "/unit_test/store", loc.upload_store.str());

    EXPECT_TRUE(loc.has_error_pages);
    server::ErrorPagesMap::const_iterator it =
        loc.error_pages.find(http::HttpStatus::SERVER_ERROR);
    ASSERT_TRUE(it != loc.error_pages.end());
    EXPECT_EQ("http://example.com/err500", it->second);

    EXPECT_TRUE(loc.has_index_pages);
    ASSERT_EQ(2u, loc.index_pages.size());
    EXPECT_EQ("up.html", loc.index_pages[0]);
    EXPECT_EQ("a.html", loc.index_pages[1]);
}

TEST(ServerConfig, RejectsDuplicateListenConfig)
{
    expectParseErrorConfigFile("duplicate_listen_err.conf");
}

TEST(ServerConfig, RejectsEmptyServerBlockConfig)
{
    expectParseErrorConfigFile("empty_server_block_err.conf");
}

TEST(ServerConfig, RejectsInvalidListenIpConfig)
{
    expectParseErrorConfigFile("invalid_listen_ip_err.conf");
}

TEST(ServerConfig, RejectsInvalidListenMissingPortConfig)
{
    expectParseErrorConfigFile("invalid_listen_missing_port_err.conf");
}

TEST(ServerConfig, RejectsInvalidListenTooBigPortConfig)
{
    expectParseErrorConfigFile("invalid_listen_port_too_big_err.conf");
}

TEST(ServerConfig, RejectsInvalidListenZeroPortConfig)
{
    expectParseErrorConfigFile("invalid_listen_port_zero_err.conf");
}

TEST(ServerConfig, RejectsMissingListenDirectiveConfig)
{
    expectParseErrorConfigFile("missing_listen_err.conf");
}

TEST(ServerConfig, RejectsMissingRootDirectiveConfig)
{
    expectParseErrorConfigFile("missing_root_err.conf");
}

TEST(ServerConfig, RejectsServerNameNoArgsConfig)
{
    expectParseErrorConfigFile("server_name_no_args_err.conf");
}

TEST(ServerConfig, RejectsEmptyFileConfig)
{
    expectParseErrorConfigFile("empty_file_err.conf");
}

TEST(ServerConfig, RejectsUnexpectedTokenConfig)
{
    expectParseErrorConfigFile("unexpected_token_err.conf");
}

TEST(ServerConfig, RejectsUnexpectedClosingBraceConfig)
{
    expectParseErrorConfigFile("unexpected_closing_brace_err.conf");
}

TEST(ServerConfig, RejectsServerMissingOpenBraceConfig)
{
    expectParseErrorConfigFile("server_missing_open_brace_err.conf");
}

TEST(ServerConfig, RejectsServerMissingCloseBraceConfig)
{
    expectParseErrorConfigFile("server_missing_close_brace_err.conf");
}

TEST(ServerConfig, RejectsTrailingTokenAfterServerConfig)
{
    expectParseErrorConfigFile("trailing_token_after_server_err.conf");
}

TEST(ServerConfig, RejectsUnknownServerDirectiveConfig)
{
    expectParseErrorConfigFile("unknown_server_directive_err.conf");
}

TEST(ServerConfig, RejectsListenMissingSemicolonConfig)
{
    expectParseErrorConfigFile("listen_missing_semicolon_err.conf");
}

TEST(ServerConfig, RejectsListenNoArgsConfig)
{
    expectParseErrorConfigFile("listen_no_args_err.conf");
}

TEST(ServerConfig, RejectsListenLocalhostConfig)
{
    expectParseErrorConfigFile("listen_localhost_err.conf");
}

TEST(ServerConfig, RejectsInvalidServerNameConfig)
{
    expectParseErrorConfigFile("invalid_server_name_err.conf");
}

TEST(ServerConfig, RejectsDuplicateServerNameOnSamePortConfig)
{
    expectParseErrorConfigFile("duplicate_server_name_same_port_err.conf");
}

TEST(ServerConfig, RejectsIndexNoArgsConfig)
{
    expectParseErrorConfigFile("index_no_args_err.conf");
}

TEST(ServerConfig, RejectsClientMaxBodySizeInvalidSuffixConfig)
{
    expectParseErrorConfigFile("client_max_body_size_invalid_suffix_err.conf");
}

TEST(ServerConfig, RejectsClientMaxBodySizeTooLargeConfig)
{
    expectParseErrorConfigFile("client_max_body_size_too_large_err.conf");
}

TEST(ServerConfig, RejectsErrorPageNonErrorStatusConfig)
{
    expectParseErrorConfigFile("error_page_non_error_status_err.conf");
}

TEST(ServerConfig, RejectsErrorPageInvalidStatusTokenConfig)
{
    expectParseErrorConfigFile("error_page_invalid_status_token_err.conf");
}

TEST(ServerConfig, RejectsErrorPageMissingArgsConfig)
{
    expectParseErrorConfigFile("error_page_missing_args_err.conf");
}

TEST(ServerConfig, RejectsLocationPathNotSlashConfig)
{
    expectParseErrorConfigFile("location_path_not_slash_err.conf");
}

TEST(ServerConfig, RejectsLocationMissingOpenBraceConfig)
{
    expectParseErrorConfigFile("location_missing_open_brace_err.conf");
}

TEST(ServerConfig, RejectsUnknownLocationDirectiveConfig)
{
    expectParseErrorConfigFile("unknown_location_directive_err.conf");
}

TEST(ServerConfig, RejectsAllowMethodsInvalidMethodConfig)
{
    expectParseErrorConfigFile("allow_methods_invalid_method_err.conf");
}

TEST(ServerConfig, RejectsAllowMethodsNoArgsConfig)
{
    expectParseErrorConfigFile("allow_methods_no_args_err.conf");
}

TEST(ServerConfig, RejectsCgiExtensionInvalidExtConfig)
{
    expectParseErrorConfigFile("cgi_extension_invalid_ext_err.conf");
}

TEST(ServerConfig, RejectsCgiExtensionMissingArgsConfig)
{
    expectParseErrorConfigFile("cgi_extension_missing_args_err.conf");
}

TEST(ServerConfig, RejectsReturnNon3xxConfig)
{
    expectParseErrorConfigFile("return_non_3xx_err.conf");
}

TEST(ServerConfig, RejectsReturnInvalidUrlConfig)
{
    expectParseErrorConfigFile("return_invalid_url_err.conf");
}

TEST(ServerConfig, RejectsReturnMissingArgsConfig)
{
    expectParseErrorConfigFile("return_missing_args_err.conf");
}

TEST(ServerConfig, RejectsAutoindexInvalidTokenConfig)
{
    expectParseErrorConfigFile("autoindex_invalid_token_err.conf");
}

TEST(ServerConfig, RejectsDuplicateLocationRootConfig)
{
    expectParseErrorConfigFile("duplicate_location_root_err.conf");
}

TEST(ServerConfig, RejectsDuplicateLocationReturnConfig)
{
    expectParseErrorConfigFile("duplicate_location_return_err.conf");
}
