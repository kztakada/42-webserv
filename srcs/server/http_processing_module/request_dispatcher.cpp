#include "server/http_processing_module/request_dispatcher.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>

#include "server/http_processing_module/request_router/request_router.hpp"
#include "server/session/fd_session/http_session/actions/execute_cgi_action.hpp"
#include "server/session/fd_session/http_session/actions/process_request_action.hpp"
#include "server/session/fd_session/http_session/actions/send_error_action.hpp"
#include "server/session/fd_session/http_session/session_context.hpp"
#include "utils/log.hpp"

namespace server
{
using namespace utils::result;

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

RequestDispatcher::RequestDispatcher(const RequestRouter& router)
    : router_(router)
{
}

Result<void> RequestDispatcher::consumeFromRecvBuffer(SessionContext& ctx)
{
    return ctx.request_handler.consumeFromRecvBuffer(ctx.recv_buffer);
}

Result<IRequestAction*> RequestDispatcher::dispatch(SessionContext& ctx)
{
    Result<void> ready = ctx.request_handler.onRequestReady();
    if (ready.isError())
    {
        utils::Log::error("RequestDispatcher",
            "onRequestReady failed:", ready.getErrorMessage());
        return new SendErrorAction(http::HttpStatus::BAD_REQUEST);
    }

    Result<void> fu = finalizeUploadStoreIfNeeded_(ctx);
    if (fu.isError())
    {
        utils::Log::error("RequestDispatcher",
            "finalizeUploadStore failed:", fu.getErrorMessage());
        return new SendErrorAction(http::HttpStatus::BAD_REQUEST);
    }

    if (ctx.request_handler.getNextStep() == HttpRequestHandler::EXECUTE_CGI)
    {
        return new ExecuteCgiAction();
    }

    return new ProcessRequestAction();
}

Result<void> RequestDispatcher::finalizeUploadStoreIfNeeded_(
    SessionContext& ctx)
{
    if (ctx.request.getMethod() != http::HttpMethod::POST)
        return Result<void>();
    if (ctx.request.getContentType() != http::ContentType::MULTIPART_FORM_DATA)
        return Result<void>();

    Result<LocationRouting> route = router_.route(ctx.request,
        ctx.socket_fd.getServerIp(), ctx.socket_fd.getServerPort());
    if (route.isError())
        return Result<void>(ERROR, route.getErrorMessage());
    if (route.unwrap().getNextAction() != STORE_BODY)
        return Result<void>();

    Result<UploadContext> up = route.unwrap().getUploadContext();
    if (up.isError())
        return Result<void>(ERROR, up.getErrorMessage());
    const UploadContext uctx = up.unwrap();

    const std::string boundary = ctx.request.getContentTypeParam("boundary");
    if (boundary.empty())
        return Result<void>(ERROR, "missing multipart boundary");

    (void)ctx.request_handler.bodyStore().finish();
    Result<int> in_fd_r = ctx.request_handler.bodyStore().openForRead();
    if (in_fd_r.isError())
        return Result<void>(ERROR, in_fd_r.getErrorMessage());
    int in_fd = in_fd_r.unwrap();

    int flags = O_WRONLY | O_CREAT;
    if (uctx.allow_overwrite)
        flags |= O_TRUNC;
    else
        flags |= O_EXCL;
    const std::string dest_path = uctx.destination_path.str();
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

}  // namespace server
