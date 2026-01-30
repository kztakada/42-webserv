#include "network/port_type.hpp"

#include <gtest/gtest.h>

TEST(PortType, DefaultsToEmpty)
{
    PortType p;
    EXPECT_TRUE(p.empty());
    EXPECT_TRUE(p.toString().empty());
}

TEST(PortType, ParseNumericAcceptsValidPort)
{
    utils::result::Result<PortType> r = PortType::parseNumeric("8080");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("8080", r.unwrap().toString());
}

TEST(PortType, ParseNumericNormalizesLeadingZeros)
{
    utils::result::Result<PortType> r = PortType::parseNumeric("00080");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("80", r.unwrap().toString());
}

TEST(PortType, ParseNumericRejectsNonNumeric)
{
    EXPECT_TRUE(PortType::parseNumeric("80a").isError());
    EXPECT_TRUE(PortType::parseNumeric(" 80").isError());
    EXPECT_TRUE(PortType::parseNumeric("-1").isError());
}

TEST(PortType, ParseNumericRejectsOutOfRange)
{
    EXPECT_TRUE(PortType::parseNumeric("0").isError());
    EXPECT_TRUE(PortType::parseNumeric("65536").isError());
}

TEST(PortType, ConstructFromSockaddrInConvertsToString)
{
    sockaddr_in addr = sockaddr_in();
    addr.sin_port = htons(8080);

    PortType p(addr);
    EXPECT_EQ("8080", p.toString());
}
