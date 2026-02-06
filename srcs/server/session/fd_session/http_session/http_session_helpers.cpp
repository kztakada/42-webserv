#include <fcntl.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>

#include "server/session/fd_session/http_session.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

http::HttpResponseEncoder::Options HttpSession::makeEncoderOptions_(
    const http::HttpRequest& request)
{
    http::HttpResponseEncoder::Options opt;
    opt.request_minor_version = request.getMinorVersion();
    opt.request_should_keep_alive = request.shouldKeepAlive();
    opt.request_is_head = (request.getMethod() == http::HttpMethod::HEAD);
    return opt;
}

Result<void> HttpSession::setSimpleErrorResponse_(
    http::HttpResponse& response, http::HttpStatus status)
{
    response.reset();
    Result<void> s = response.setStatus(status);
    if (s.isError())
        return s;

    // ボディは別レイヤで作る想定。ここではヘッダだけ。
    response.removeHeader(
        http::HeaderName(http::HeaderName::TRANSFER_ENCODING).toString());
    response.removeHeader(
        http::HeaderName(http::HeaderName::CONTENT_LENGTH).toString());
    return Result<void>();
}

// RequestProcessor::Output を使って body_source_ / response_writer_
// を差し替える。 既存の writer/body_source は必要に応じて破棄する。
void HttpSession::installBodySourceAndWriter_(BodySource* body_source)
{
    if (response_writer_ != NULL)
    {
        delete response_writer_;
        response_writer_ = NULL;
    }
    if (body_source_ != NULL)
    {
        delete body_source_;
        body_source_ = NULL;
    }

    body_source_ = body_source;

    http::HttpResponseEncoder::Options opt = makeEncoderOptions_(request_);
    response_writer_ = new HttpResponseWriter(response_, opt, body_source_);
}

namespace
{

static Result<void> writeAll_(int fd, const std::string& data)
{
    if (data.empty())
        return Result<void>();

    size_t off = 0;
    while (off < data.size())
    {
        const size_t chunk = data.size() - off;
        const ssize_t w = ::write(fd, data.data() + off, chunk);
        if (w < 0)
            return Result<void>(ERROR, "write() failed");
        if (w == 0)
            return Result<void>(ERROR, "write() returned 0");
        off += static_cast<size_t>(w);
    }
    return Result<void>();
}

static std::string toLowerAscii_(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i)
    {
        out[i] =
            static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}

class MultipartStream
{
   public:
    explicit MultipartStream(int fd) : fd_(fd), buf_() {}

    Result<bool> readLine(std::string* out)
    {
        if (out == NULL)
            return Result<bool>(ERROR, "null output");
        for (;;)
        {
            std::string::size_type pos = buf_.find("\r\n");
            if (pos != std::string::npos)
            {
                *out = buf_.substr(0, pos);
                buf_.erase(0, pos + 2);
                return true;
            }
            Result<bool> f = fill_();
            if (f.isError())
                return f;
            if (!f.unwrap())
                return false;
        }
    }

    Result<bool> readUntilBoundary(
        const std::string& boundary, int out_fd, bool* is_final)
    {
        if (is_final == NULL)
            return Result<bool>(ERROR, "null is_final");

        const std::string delimiter = "\r\n--" + boundary;
        for (;;)
        {
            std::string::size_type pos = buf_.find(delimiter);
            if (pos != std::string::npos)
            {
                Result<void> w = (out_fd >= 0)
                                     ? writeAll_(out_fd, buf_.substr(0, pos))
                                     : Result<void>();
                if (w.isError())
                    return Result<bool>(ERROR, w.getErrorMessage());

                buf_.erase(0, pos + delimiter.size());

                // boundary suffix: "--"(final) or "\r\n"(next part)
                while (buf_.size() < 2)
                {
                    Result<bool> f = fill_();
                    if (f.isError())
                        return f;
                    if (!f.unwrap())
                        return false;
                }

                if (buf_.substr(0, 2) == "--")
                {
                    *is_final = true;
                    buf_.erase(0, 2);
                    if (buf_.size() >= 2 && buf_.substr(0, 2) == "\r\n")
                        buf_.erase(0, 2);
                    return true;
                }
                if (buf_.substr(0, 2) == "\r\n")
                {
                    *is_final = false;
                    buf_.erase(0, 2);
                    return true;
                }

                return Result<bool>(ERROR, "invalid boundary suffix");
            }

            const size_t keep = delimiter.size();
            if (buf_.size() > keep)
            {
                const size_t safe_len = buf_.size() - keep;
                Result<void> w =
                    (out_fd >= 0) ? writeAll_(out_fd, buf_.substr(0, safe_len))
                                  : Result<void>();
                if (w.isError())
                    return Result<bool>(ERROR, w.getErrorMessage());
                buf_.erase(0, safe_len);
            }

            Result<bool> f = fill_();
            if (f.isError())
                return f;
            if (!f.unwrap())
                return false;
        }
    }

   private:
    Result<bool> fill_()
    {
        char buf[8192];
        const ssize_t n = ::read(fd_, buf, sizeof(buf));
        if (n < 0)
            return Result<bool>(ERROR, "read() failed");
        if (n == 0)
            return false;
        buf_.append(buf, static_cast<size_t>(n));
        return true;
    }

    int fd_;
    std::string buf_;
};

static bool isFilePartHeader_(const std::string& line)
{
    std::string lower = toLowerAscii_(line);
    if (lower.find("content-disposition:") != 0)
        return false;
    return (lower.find("filename=") != std::string::npos);
}

static void cleanupDestinationFile_(const std::string& path)
{
    if (path.empty())
        return;
    (void)std::remove(path.c_str());
}

}  // namespace

// processError を試み、失敗したら setSimpleErrorResponse_
// で最低限のヘッダだけ作る。 fallback の場合 out->body_source は NULL
// になる。
Result<void> HttpSession::buildErrorOutput_(
    http::HttpStatus status, RequestProcessor::Output* out)
{
    Result<RequestProcessor::Output> per =
        processor_.processError(request_, status, response_);
    if (per.isOk())
    {
        *out = per.unwrap();
        return Result<void>();
    }

    Result<void> er = setSimpleErrorResponse_(response_, status);
    if (er.isError())
        return er;

    out->body_source = NULL;
    out->should_close_connection = false;
    return Result<void>();
}

// upload_store + multipart/form-data の場合、一時ファイルに保存された
// multipart envelope から file part だけを抽出し、destination に保存する。
Result<void> HttpSession::finalizeUploadStoreIfNeeded_()
{
    if (request_.getMethod() != http::HttpMethod::POST)
        return Result<void>();
    if (request_.getContentType() != http::ContentType::MULTIPART_FORM_DATA)
        return Result<void>();

    Result<LocationRouting> route = router_.route(
        request_, socket_fd_.getServerIp(), socket_fd_.getServerPort());
    if (route.isError())
        return Result<void>(ERROR, route.getErrorMessage());
    if (route.unwrap().getNextAction() != STORE_BODY)
        return Result<void>();

    Result<UploadContext> up = route.unwrap().getUploadContext();
    if (up.isError())
        return Result<void>(ERROR, up.getErrorMessage());
    const UploadContext ctx = up.unwrap();

    const std::string boundary = request_.getContentTypeParam("boundary");
    if (boundary.empty())
        return Result<void>(ERROR, "missing multipart boundary");

    (void)handler_.bodyStore().finish();
    Result<int> in_fd_r = handler_.bodyStore().openForRead();
    if (in_fd_r.isError())
        return Result<void>(ERROR, in_fd_r.getErrorMessage());
    int in_fd = in_fd_r.unwrap();

    int flags = O_WRONLY | O_CREAT;
    if (ctx.allow_overwrite)
        flags |= O_TRUNC;
    else
        flags |= O_EXCL;
    const std::string dest_path = ctx.destination_path.str();
    const int out_fd = ::open(dest_path.c_str(), flags, 0644);
    if (out_fd < 0)
    {
        ::close(in_fd);
        return Result<void>(ERROR, "open() failed");
    }

    MultipartStream ms(in_fd);
    const std::string boundary_line = "--" + boundary;
    const std::string boundary_line_final = boundary_line + "--";

    bool found_first_boundary = false;
    for (;;)
    {
        std::string line;
        Result<bool> lr = ms.readLine(&line);
        if (lr.isError())
        {
            ::close(out_fd);
            ::close(in_fd);
            cleanupDestinationFile_(dest_path);
            return Result<void>(ERROR, lr.getErrorMessage());
        }
        if (!lr.unwrap())
            break;
        if (line == boundary_line)
        {
            found_first_boundary = true;
            break;
        }
        if (line == boundary_line_final)
            break;
    }

    if (!found_first_boundary)
    {
        ::close(out_fd);
        ::close(in_fd);
        cleanupDestinationFile_(dest_path);
        return Result<void>(ERROR, "multipart boundary not found");
    }

    bool wrote_file = false;
    for (;;)
    {
        // headers
        bool is_file = false;
        for (;;)
        {
            std::string line;
            Result<bool> lr = ms.readLine(&line);
            if (lr.isError())
            {
                ::close(out_fd);
                ::close(in_fd);
                cleanupDestinationFile_(dest_path);
                return Result<void>(ERROR, lr.getErrorMessage());
            }
            if (!lr.unwrap())
            {
                ::close(out_fd);
                ::close(in_fd);
                cleanupDestinationFile_(dest_path);
                return Result<void>(ERROR, "unexpected EOF");
            }
            if (line.empty())
                break;
            if (isFilePartHeader_(line))
                is_file = true;
        }

        bool is_final = false;
        Result<bool> dr = ms.readUntilBoundary(
            boundary, (is_file && !wrote_file) ? out_fd : -1, &is_final);
        if (dr.isError())
        {
            ::close(out_fd);
            ::close(in_fd);
            cleanupDestinationFile_(dest_path);
            return Result<void>(ERROR, dr.getErrorMessage());
        }
        if (!dr.unwrap())
        {
            ::close(out_fd);
            ::close(in_fd);
            cleanupDestinationFile_(dest_path);
            return Result<void>(ERROR, "unexpected EOF");
        }

        if (is_file && !wrote_file)
        {
            wrote_file = true;
            break;
        }

        if (is_final)
            break;
    }

    ::close(out_fd);
    ::close(in_fd);

    if (!wrote_file)
    {
        cleanupDestinationFile_(dest_path);
        return Result<void>(ERROR, "no file part found");
    }
    return Result<void>();
}

// handler_.onRequestReady() を実行し、失敗時は 400
// の最小レスポンスを用意する。 どちらの場合でも response_ の HTTP version
// は request_ に合わせて設定する。
Result<void> HttpSession::prepareRequestReadyOrBadRequest_()
{
    Result<void> ready = handler_.onRequestReady();
    if (ready.isError())
    {
        Result<void> er =
            setSimpleErrorResponse_(response_, http::HttpStatus::BAD_REQUEST);
        if (er.isError())
            return er;
    }
    response_.setHttpVersion(request_.getHttpVersion());
    return Result<void>();
}

// RequestProcessor::process を実行し、失敗したら 500 を用意する。
// この場合は接続を close する方針なので out->should_close_connection を
// true にする。
Result<void> HttpSession::buildProcessorOutputOrServerError_(
    RequestProcessor::Output* out)
{
    Result<RequestProcessor::Output> pr =
        processor_.process(request_, response_);
    if (pr.isOk())
    {
        *out = pr.unwrap();
        return Result<void>();
    }

    utils::Log::error("RequestProcessor error: ", pr.getErrorMessage());

    Result<void> bo = buildErrorOutput_(http::HttpStatus::SERVER_ERROR, out);
    if (bo.isError())
        return bo;

    response_.setHttpVersion(request_.getHttpVersion());
    out->should_close_connection = true;
    return Result<void>();
}

}  // namespace server
