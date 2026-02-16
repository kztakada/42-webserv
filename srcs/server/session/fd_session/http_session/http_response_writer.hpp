#ifndef WEBSERV_HTTP_RESPONSE_WRITER_HPP_
#define WEBSERV_HTTP_RESPONSE_WRITER_HPP_

#include <cstddef>

#include "http/http_response_encoder.hpp"
#include "server/session/fd_session/http_session/body_source.hpp"
#include "server/session/io_buffer.hpp"
#include "utils/result.hpp"

namespace server
{

using namespace utils::result;

class HttpResponseWriter
{
   public:
    enum Step
    {
        NEED_MORE,
        DONE
    };

    struct PumpResult
    {
        Step step;
        bool should_close_connection;

        PumpResult() : step(NEED_MORE), should_close_connection(false) {}
    };

    HttpResponseWriter(http::HttpResponse& response,
        const http::HttpResponseEncoder::Options& options, BodySource* body);
    ~HttpResponseWriter();

    // send_buffer に追加できる分だけエンコードして積む（ソケットwriteは別）
    Result<PumpResult> pump(IoBuffer& send_buffer);

    // エラー等で body をこれ以上送れない場合に、可能ならレスポンスの終端を
    // send_buffer に積む。chunked の場合は "0\r\n\r\n" を送る。
    // close-delimited の場合は何も積まれない（接続 close が EOF）。
    Result<void> writeEof(IoBuffer& send_buffer);

   private:
    static const size_t kDefaultChunkBytes = 8192;

    http::HttpResponse& response_;
    http::HttpResponseEncoder encoder_;
    BodySource* body_;

    bool header_written_;
    bool eof_written_;

    HttpResponseWriter();
    HttpResponseWriter(const HttpResponseWriter& rhs);
    HttpResponseWriter& operator=(const HttpResponseWriter& rhs);
};

}  // namespace server

#endif
