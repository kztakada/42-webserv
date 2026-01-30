#include "network/ip_address.hpp"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>

TEST(IPAddress, DefaultsToEmpty)
{
    IPAddress ip;
    EXPECT_TRUE(ip.empty());
    EXPECT_TRUE(ip.toString().empty());
}

TEST(IPAddress, Ipv4AnyReturnsAnyAddress)
{
    IPAddress ip = IPAddress::ipv4Any();
    EXPECT_FALSE(ip.empty());
    EXPECT_EQ(std::string("0.0.0.0"), ip.toString());
}

TEST(IPAddress, ParseIpv4NumericAcceptsValidAddress)
{
    utils::result::Result<IPAddress> r =
        IPAddress::parseIpv4Numeric("127.0.0.1");
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(std::string("127.0.0.1"), r.unwrap().toString());
}

TEST(IPAddress, ParseIpv4NumericNormalizesLeadingZeros)
{
    utils::result::Result<IPAddress> r =
        IPAddress::parseIpv4Numeric("001.002.003.004");
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(std::string("1.2.3.4"), r.unwrap().toString());
}

TEST(IPAddress, ParseIpv4NumericRejectsInvalidAddress)
{
    EXPECT_TRUE(IPAddress::parseIpv4Numeric("").isError());
    EXPECT_TRUE(IPAddress::parseIpv4Numeric("localhost").isError());
    EXPECT_TRUE(IPAddress::parseIpv4Numeric("256.0.0.1").isError());
    EXPECT_TRUE(IPAddress::parseIpv4Numeric("1.2.3").isError());
    EXPECT_TRUE(IPAddress::parseIpv4Numeric("1.2.3.4.5").isError());
    EXPECT_TRUE(IPAddress::parseIpv4Numeric("1..2.3.4").isError());
    EXPECT_TRUE(IPAddress::parseIpv4Numeric(".1.2.3.4").isError());
    EXPECT_TRUE(IPAddress::parseIpv4Numeric("1.2.3.4.").isError());
    EXPECT_TRUE(IPAddress::parseIpv4Numeric("1.2.3.a").isError());
}

TEST(IPAddress, ConstructFromSockaddrInConvertsToString)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(0x7F000001ul);  // 127.0.0.1

    IPAddress ip(addr);
    EXPECT_EQ(std::string("127.0.0.1"), ip.toString());
}
