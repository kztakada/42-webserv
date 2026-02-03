#include "http/http_response_encoder.hpp"

#include <gtest/gtest.h>

#include "http/http_response.hpp"

namespace
{

using utils::result::Result;

static std::string toString(const std::vector<utils::Byte>& v)
{
    return std::string(reinterpret_cast<const char*>(v.data()), v.size());
}

TEST(HttpResponseEncoderTest, Http11NoContentLengthUsesChunked)
{
    http::HttpResponse res;
    ASSERT_TRUE(res.setStatus(http::HttpStatus::OK).isOk());

    http::HttpResponseEncoder::Options opt;
    opt.request_minor_version = 1;
    opt.request_should_keep_alive = true;
    opt.request_is_head = false;

    http::HttpResponseEncoder enc(opt);
    Result<std::vector<utils::Byte> > h = enc.encodeHeader(res);
    ASSERT_TRUE(h.isOk());

    const std::string hs = toString(h.unwrap());
    ASSERT_NE(hs.find("Transfer-Encoding: chunked\r\n"), std::string::npos);

    const char payload[] = "abc";
    Result<std::vector<utils::Byte> > b = enc.encodeBodyChunk(
        res, reinterpret_cast<const utils::Byte*>(payload), 3);
    ASSERT_TRUE(b.isOk());
    ASSERT_EQ(toString(b.unwrap()), std::string("3\r\nabc\r\n"));

    Result<std::vector<utils::Byte> > e = enc.encodeEof(res);
    ASSERT_TRUE(e.isOk());
    ASSERT_EQ(toString(e.unwrap()), std::string("0\r\n\r\n"));
    ASSERT_TRUE(res.isComplete());
}

TEST(HttpResponseEncoderTest, Http10NoContentLengthBecomesCloseDelimited)
{
    http::HttpResponse res;
    ASSERT_TRUE(res.setStatus(http::HttpStatus::OK).isOk());

    http::HttpResponseEncoder::Options opt;
    opt.request_minor_version = 0;
    opt.request_should_keep_alive = true;
    opt.request_is_head = false;

    http::HttpResponseEncoder enc(opt);
    Result<std::vector<utils::Byte> > h = enc.encodeHeader(res);
    ASSERT_TRUE(h.isOk());

    const std::string hs = toString(h.unwrap());
    // HTTP/1.0 は chunked できない
    ASSERT_EQ(hs.find("Transfer-Encoding: chunked\r\n"), std::string::npos);
    // close が必要
    ASSERT_TRUE(enc.shouldCloseConnection());

    const char payload[] = "abc";
    Result<std::vector<utils::Byte> > b = enc.encodeBodyChunk(
        res, reinterpret_cast<const utils::Byte*>(payload), 3);
    ASSERT_TRUE(b.isOk());
    ASSERT_EQ(toString(b.unwrap()), std::string("abc"));

    Result<std::vector<utils::Byte> > e = enc.encodeEof(res);
    ASSERT_TRUE(e.isOk());
    ASSERT_EQ(toString(e.unwrap()), std::string(""));
    ASSERT_TRUE(res.isComplete());
}

}  // namespace
