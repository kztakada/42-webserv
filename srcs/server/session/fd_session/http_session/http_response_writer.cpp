#include "server/session/fd_session/http_session/http_response_writer.hpp"

namespace server
{

const size_t HttpResponseWriter::kDefaultChunkBytes;

HttpResponseWriter::HttpResponseWriter(http::HttpResponse& response,
    const http::HttpResponseEncoder::Options& options, BodySource* body)
    : response_(response),
      encoder_(options),
      body_(body),
      header_written_(false),
      eof_written_(false)
{
}

HttpResponseWriter::~HttpResponseWriter() {}

Result<HttpResponseWriter::PumpResult> HttpResponseWriter::pump(
    IoBuffer& send_buffer)
{
    PumpResult pr;

    if (!header_written_)
    {
        Result<std::vector<utils::Byte> > h = encoder_.encodeHeader(response_);
        if (h.isError())
            return Result<PumpResult>(ERROR, h.getErrorMessage());

        const std::vector<utils::Byte>& bytes = h.unwrap();
        if (!bytes.empty())
            send_buffer.append(
                reinterpret_cast<const char*>(&bytes[0]), bytes.size());

        header_written_ = true;
        pr.should_close_connection = encoder_.shouldCloseConnection();

        if (response_.isComplete())
        {
            pr.step = DONE;
            return pr;
        }
    }

    pr.should_close_connection = encoder_.shouldCloseConnection();

    if (eof_written_)
    {
        pr.step = DONE;
        return pr;
    }

    // body source がない場合は EOF のみ
    if (body_ == NULL)
    {
        Result<std::vector<utils::Byte> > eof = encoder_.encodeEof(response_);
        if (eof.isError())
            return Result<PumpResult>(ERROR, eof.getErrorMessage());

        const std::vector<utils::Byte>& bytes = eof.unwrap();
        if (!bytes.empty())
            send_buffer.append(
                reinterpret_cast<const char*>(&bytes[0]), bytes.size());

        eof_written_ = true;
        pr.step = DONE;
        return pr;
    }

    // 読み出し
    Result<BodySource::ReadResult> rr = body_->read(kDefaultChunkBytes);
    if (rr.isError())
        return Result<PumpResult>(ERROR, rr.getErrorMessage());

    BodySource::ReadResult r = rr.unwrap();

    if (r.status == BodySource::READ_WOULD_BLOCK)
    {
        pr.step = NEED_MORE;
        return pr;
    }

    if (!r.data.empty())
    {
        Result<std::vector<utils::Byte> > enc =
            encoder_.encodeBodyChunk(response_, &r.data[0], r.data.size());
        if (enc.isError())
            return Result<PumpResult>(ERROR, enc.getErrorMessage());

        const std::vector<utils::Byte>& bytes = enc.unwrap();
        if (!bytes.empty())
            send_buffer.append(
                reinterpret_cast<const char*>(&bytes[0]), bytes.size());
    }

    if (r.status == BodySource::READ_EOF)
    {
        Result<std::vector<utils::Byte> > eof = encoder_.encodeEof(response_);
        if (eof.isError())
            return Result<PumpResult>(ERROR, eof.getErrorMessage());

        const std::vector<utils::Byte>& bytes = eof.unwrap();
        if (!bytes.empty())
            send_buffer.append(
                reinterpret_cast<const char*>(&bytes[0]), bytes.size());

        eof_written_ = true;
        pr.step = DONE;
        return pr;
    }

    pr.step = NEED_MORE;
    return pr;
}

Result<void> HttpResponseWriter::writeEof(IoBuffer& send_buffer)
{
    if (eof_written_ || response_.isComplete())
        return Result<void>();

    if (!header_written_)
    {
        Result<std::vector<utils::Byte> > h = encoder_.encodeHeader(response_);
        if (h.isError())
            return Result<void>(ERROR, h.getErrorMessage());

        const std::vector<utils::Byte>& bytes = h.unwrap();
        if (!bytes.empty())
            send_buffer.append(
                reinterpret_cast<const char*>(&bytes[0]), bytes.size());

        header_written_ = true;
    }

    Result<std::vector<utils::Byte> > eof = encoder_.encodeEof(response_);
    if (eof.isError())
        return Result<void>(ERROR, eof.getErrorMessage());

    const std::vector<utils::Byte>& bytes = eof.unwrap();
    if (!bytes.empty())
        send_buffer.append(
            reinterpret_cast<const char*>(&bytes[0]), bytes.size());

    eof_written_ = true;
    return Result<void>();
}

}  // namespace server
