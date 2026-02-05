#include <gtest/gtest.h>

#include <map>
#include <string>

#include "http/content_types.hpp"

TEST(ContentType, ParseHeaderValueParsesBoundaryParam)
{
    std::map<std::string, std::string> params;
    http::ContentType t = http::ContentType::parseHeaderValue(
        "multipart/form-data; boundary=abc123", &params);

    EXPECT_EQ(http::ContentType::MULTIPART_FORM_DATA, t);
    EXPECT_EQ(std::string("abc123"), params["boundary"]);
}

TEST(ContentType, ParseHeaderValueTrimsAndUnquotesParams)
{
    std::map<std::string, std::string> params;
    http::ContentType t = http::ContentType::parseHeaderValue(
        "Multipart/Form-Data ;  Boundary=\"XYZ\" ", &params);

    EXPECT_EQ(http::ContentType::MULTIPART_FORM_DATA, t);
    EXPECT_EQ(std::string("XYZ"), params["boundary"]);
}

TEST(ContentType, ParseHeaderValueWorksWithoutParams)
{
    std::map<std::string, std::string> params;
    http::ContentType t =
        http::ContentType::parseHeaderValue("text/plain", &params);

    EXPECT_EQ(http::ContentType::TEXT_PLAIN, t);
    EXPECT_TRUE(params.empty());
}
