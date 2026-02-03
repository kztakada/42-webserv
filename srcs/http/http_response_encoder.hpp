#ifndef HTTP_HTTP_RESPONSE_ENCODER_HPP_
#define HTTP_HTTP_RESPONSE_ENCODER_HPP_

#include <string>
#include <vector>

#include "http/http_response.hpp"
#include "utils/byte.hpp"
#include "utils/result.hpp"

namespace http
{

using namespace utils::result;

class HttpResponseEncoder
{
   public:
    enum BodyMode
    {
        kNoBody,
        kContentLength,
        kChunked,
        kCloseDelimited
    };

    struct Options
    {
        int request_minor_version;       // 0: HTTP/1.0, 1: HTTP/1.1
        bool request_should_keep_alive;  // HttpRequest::shouldKeepAlive()
        bool request_is_head;            // method == HEAD

        Options()
            : request_minor_version(1),
              request_should_keep_alive(true),
              request_is_head(false)
        {
        }
    };

    explicit HttpResponseEncoder(const Options& options);
    ~HttpResponseEncoder();

    Result<std::vector<utils::Byte> > encodeHeader(HttpResponse& response);
    Result<std::vector<utils::Byte> > encodeBodyChunk(
        HttpResponse& response, const utils::Byte* data, size_t len);
    Result<std::vector<utils::Byte> > encodeEof(HttpResponse& response);

    BodyMode bodyMode() const;
    bool shouldCloseConnection() const;

   private:
    Options options_;
    BodyMode body_mode_;
    bool decided_;
    bool should_close_connection_;

    unsigned long expected_content_length_;
    unsigned long body_bytes_sent_;

    Result<void> decide_(HttpResponse& response);
    static bool isBodyForbiddenStatus_(HttpStatus status);
    static std::string reasonOrDefault_(const HttpResponse& response);

    static void appendString_(
        std::vector<utils::Byte>& out, const std::string& s);
    static void appendCrlf_(std::vector<utils::Byte>& out);
    static std::string toHex_(size_t n);
};

}  // namespace http

#endif
