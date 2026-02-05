#include <gtest/gtest.h>

#include "http/status.hpp"

TEST(HttpStatus, LengthRequiredMessageAndCode)
{
    http::HttpStatus st(http::HttpStatus::LENGTH_REQUIRED);
    EXPECT_EQ(411ul, st.toInt());
    EXPECT_STREQ("Length Required", st.getMessage());
}

TEST(HttpStatus, BadGatewayMessageAndCode)
{
    http::HttpStatus st(http::HttpStatus::BAD_GATEWAY);
    EXPECT_EQ(502ul, st.toInt());
    EXPECT_STREQ("Bad Gateway", st.getMessage());
}

TEST(HttpStatus, GatewayTimeoutMessageAndCode)
{
    http::HttpStatus st(http::HttpStatus::GATEWAY_TIMEOUT);
    EXPECT_EQ(504ul, st.toInt());
    EXPECT_STREQ("Gateway Timeout", st.getMessage());
}
