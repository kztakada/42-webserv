#include "http/http_method.hpp"

#include <gtest/gtest.h>

namespace
{

struct MethodCase
{
    const char* name;
    http::HttpMethod::Type type;
};

}  // namespace

TEST(HttpMethod, FromStringCoversAllDefinedTypes)
{
    static const MethodCase kCases[] = {
        {"GET", http::HttpMethod::GET},
        {"POST", http::HttpMethod::POST},
        {"PUT", http::HttpMethod::PUT},
        {"DELETE", http::HttpMethod::DELETE},
        {"HEAD", http::HttpMethod::HEAD},
        {"OPTIONS", http::HttpMethod::OPTIONS},
        {"PATCH", http::HttpMethod::PATCH},
        {"CONNECT", http::HttpMethod::CONNECT},
        {"TRACE", http::HttpMethod::TRACE},
    };

    for (size_t i = 0; i < sizeof(kCases) / sizeof(kCases[0]); ++i)
    {
        EXPECT_EQ(kCases[i].type, http::HttpMethod::fromString(kCases[i].name));
        EXPECT_EQ(kCases[i].type,
            http::HttpMethod::fromString(std::string(kCases[i].name)).unwrap());

        if (kCases[i].type == http::HttpMethod::UNKNOWN)
        {
            EXPECT_FALSE(http::HttpMethod::isValid(kCases[i].name));
            EXPECT_FALSE(
                http::HttpMethod::isValid(std::string(kCases[i].name)));
        }
        else
        {
            EXPECT_TRUE(http::HttpMethod::isValid(kCases[i].name));
            EXPECT_TRUE(http::HttpMethod::isValid(std::string(kCases[i].name)));
        }
    }
}

TEST(HttpMethod, ToStringCoversAllDefinedTypes)
{
    static const MethodCase kCases[] = {
        {"GET", http::HttpMethod::GET},
        {"POST", http::HttpMethod::POST},
        {"PUT", http::HttpMethod::PUT},
        {"DELETE", http::HttpMethod::DELETE},
        {"HEAD", http::HttpMethod::HEAD},
        {"OPTIONS", http::HttpMethod::OPTIONS},
        {"PATCH", http::HttpMethod::PATCH},
        {"CONNECT", http::HttpMethod::CONNECT},
        {"TRACE", http::HttpMethod::TRACE},
        {"UNKNOWN", http::HttpMethod::UNKNOWN},
    };

    for (size_t i = 0; i < sizeof(kCases) / sizeof(kCases[0]); ++i)
    {
        http::HttpMethod m(kCases[i].type);
        EXPECT_STREQ(kCases[i].name, m.c_str());
        EXPECT_EQ(std::string(kCases[i].name), m.toString());
    }
}

TEST(HttpMethod, UnknownMethodsBecomeUnknown)
{
    EXPECT_EQ(http::HttpMethod::UNKNOWN, http::HttpMethod::fromString(NULL));

    EXPECT_EQ(http::HttpMethod::UNKNOWN, http::HttpMethod::fromString(""));
    EXPECT_EQ(http::HttpMethod::UNKNOWN, http::HttpMethod::fromString("get"));
    EXPECT_EQ(
        http::HttpMethod::UNKNOWN, http::HttpMethod::fromString("PROPFIND"));
    EXPECT_EQ(http::HttpMethod::UNKNOWN, http::HttpMethod::fromString("MKCOL"));

    EXPECT_FALSE(http::HttpMethod::isValid(""));
    EXPECT_FALSE(http::HttpMethod::isValid("get"));
    EXPECT_FALSE(http::HttpMethod::isValid("PROPFIND"));
    EXPECT_FALSE(http::HttpMethod::isValid("MKCOL"));
}
