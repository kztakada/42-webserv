#include "server/http_processing_module/request_router/virtual_server.hpp"

#include <gtest/gtest.h>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/location_directive_conf.hpp"
#include "server/config/virtual_server_conf.hpp"

TEST(VirtualServer, ListensOnMatchesWildcardIp)
{
    server::VirtualServerConf conf;
    ASSERT_TRUE(conf.appendListen("", "8080").isOk());
    ASSERT_TRUE(conf.setRootDir("/var/www").isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(conf.appendLocation(loc).isOk());

    ASSERT_TRUE(conf.isValid());

    server::VirtualServer vs(conf);

    utils::result::Result<IPAddress> ip =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    ASSERT_TRUE(ip.isOk());
    utils::result::Result<PortType> port = PortType::parseNumeric("8080");
    ASSERT_TRUE(port.isOk());

    EXPECT_TRUE(vs.listensOn(ip.unwrap(), port.unwrap()));
}

TEST(VirtualServer, ServerNameAndDefaultAllowedMethodAreApplied)
{
    server::VirtualServerConf conf;
    ASSERT_TRUE(conf.appendListen("", "8080").isOk());
    ASSERT_TRUE(conf.setRootDir("/var/www").isOk());
    ASSERT_TRUE(conf.appendServerName("example.com").isOk());

    server::LocationDirectiveConf loc;
    ASSERT_TRUE(loc.setPathPattern("/").isOk());
    ASSERT_TRUE(conf.appendLocation(loc).isOk());

    ASSERT_TRUE(conf.isValid());

    server::VirtualServer vs(conf);

    EXPECT_TRUE(vs.isServerNameIncluded("example.com"));
    EXPECT_FALSE(vs.isServerNameIncluded("other.com"));

    const server::LocationDirective* matched = vs.findLocationByPath("/");
    ASSERT_TRUE(matched != NULL);

    EXPECT_TRUE(matched->isAllowed(http::HttpMethod::GET));
    EXPECT_FALSE(matched->isAllowed(http::HttpMethod::POST));
}
