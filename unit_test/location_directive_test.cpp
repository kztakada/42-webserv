#include "server/request_router/location_directive.hpp"

#include <gtest/gtest.h>

#include "http/http_method.hpp"
#include "server/config/location_directive_conf.hpp"

TEST(LocationDirective, ForwardMatchAndAbsolutePath)
{
    server::LocationDirectiveConf conf;
    ASSERT_TRUE(conf.setPathPattern("/cgi-bin").isOk());
    ASSERT_TRUE(conf.setRootDir("/var/www/cgi-bin").isOk());

    server::LocationDirective loc(conf);

    EXPECT_TRUE(loc.isMatchPattern("/cgi-bin/test"));
    EXPECT_FALSE(loc.isMatchPattern("/app/cgi-bin/test"));

    EXPECT_EQ(loc.getAbsolutePath("/cgi-bin/test"),
        std::string("/var/www/cgi-bin/test"));
}

TEST(LocationDirective, BackwardMatchAndAbsolutePath)
{
    server::LocationDirectiveConf conf;
    ASSERT_TRUE(conf.setPathPattern("/cgi-bin").isOk());
    ASSERT_TRUE(conf.setIsBackwardSearch(true).isOk());
    ASSERT_TRUE(conf.setRootDir("/var/www").isOk());

    server::LocationDirective loc(conf);

    EXPECT_TRUE(loc.isMatchPattern("/app/cgi-bin"));
    EXPECT_FALSE(loc.isMatchPattern("/cgi-bin/app"));

    EXPECT_EQ(loc.getAbsolutePath("/app/cgi-bin"),
        std::string("/var/www/app/cgi-bin"));
}

TEST(LocationDirective, BuildsIndexCandidatePaths)
{
    server::LocationDirectiveConf conf;
    ASSERT_TRUE(conf.setPathPattern("/docs").isOk());
    ASSERT_TRUE(conf.setRootDir("/var/www").isOk());
    ASSERT_TRUE(conf.appendIndexPage("index.html").isOk());
    ASSERT_TRUE(conf.appendIndexPage("home.html").isOk());

    server::LocationDirective loc(conf);

    std::vector<std::string> candidates =
        loc.buildIndexCandidatePaths("/docs/");
    ASSERT_EQ(candidates.size(), static_cast<size_t>(2));
    EXPECT_EQ(candidates[0], std::string("/var/www/index.html"));
    EXPECT_EQ(candidates[1], std::string("/var/www/home.html"));
}
