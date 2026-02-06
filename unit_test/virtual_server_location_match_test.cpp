#include <gtest/gtest.h>

#include "server/config/virtual_server_conf.hpp"
#include "server/http_processing_module/request_router/virtual_server.hpp"

namespace
{

TEST(VirtualServer, FindLocationByPathChoosesLongestPrefixMatch)
{
    server::VirtualServerConf conf;
    ASSERT_TRUE(conf.appendListen("0.0.0.0", "8080").isOk());
    ASSERT_TRUE(conf.setRootDir("/").isOk());

    server::LocationDirectiveConf root;
    ASSERT_TRUE(root.setPathPattern("/").isOk());
    ASSERT_TRUE(root.setRootDir("/var/www").isOk());
    ASSERT_TRUE(conf.appendLocation(root).isOk());

    server::LocationDirectiveConf images;
    ASSERT_TRUE(images.setPathPattern("/images").isOk());
    ASSERT_TRUE(images.setRootDir("/var/images").isOk());
    ASSERT_TRUE(conf.appendLocation(images).isOk());

    server::VirtualServer vserver(conf);
    const server::LocationDirective* loc =
        vserver.findLocationByPath("/images/logo.png");
    ASSERT_NE(loc, static_cast<const server::LocationDirective*>(NULL));
    EXPECT_EQ(loc->getAbsolutePath("/images/logo.png"), "/var/images/logo.png");
}

TEST(VirtualServer, FindLocationByPathKeepsFirstOnEqualLength)
{
    server::VirtualServerConf conf;
    ASSERT_TRUE(conf.appendListen("0.0.0.0", "8080").isOk());
    ASSERT_TRUE(conf.setRootDir("/").isOk());

    server::LocationDirectiveConf a;
    ASSERT_TRUE(a.setPathPattern("/same").isOk());
    ASSERT_TRUE(a.setRootDir("/a").isOk());
    ASSERT_TRUE(conf.appendLocation(a).isOk());

    server::LocationDirectiveConf b;
    ASSERT_TRUE(b.setPathPattern("/same").isOk());
    ASSERT_TRUE(b.setRootDir("/b").isOk());
    ASSERT_TRUE(conf.appendLocation(b).isOk());

    server::VirtualServer vserver(conf);
    const server::LocationDirective* loc =
        vserver.findLocationByPath("/same/file.txt");
    ASSERT_NE(loc, static_cast<const server::LocationDirective*>(NULL));
    EXPECT_EQ(loc->getAbsolutePath("/same/file.txt"), "/a/file.txt");
}

}  // namespace
