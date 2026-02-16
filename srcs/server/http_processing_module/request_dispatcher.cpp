#include "server/http_processing_module/request_dispatcher.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

#include "server/http_processing_module/request_router/request_router.hpp"
#include "server/session/fd_session/http_session/actions/execute_cgi_action.hpp"
#include "server/session/fd_session/http_session/actions/process_request_action.hpp"
#include "server/session/fd_session/http_session/actions/send_error_action.hpp"
#include "server/session/fd_session/http_session/body_store.hpp"
#include "server/session/fd_session/http_session/session_context.hpp"
#include "utils/log.hpp"
#include "utils/timestamp.hpp"

namespace server
{
using namespace utils::result;

namespace
{

static bool canWriteToDir_(const std::string& dir)
{
    return ::access(dir.c_str(), W_OK | X_OK) == 0;
}

static Result<int> openBodyStoreForReadAllowEmpty_(BodyStore& body_store)
{
    Result<int> fd = body_store.openForRead();
    if (fd.isOk())
        return fd;

    // Content-Length: 0 等で body_store が一度も open されず、
    // 一時ファイルが未作成な場合がある。その場合も空ファイルとして扱える。
    if (body_store.size() != 0)
        return fd;

    Result<void> b = body_store.begin();
    if (b.isError())
        return Result<int>(ERROR, -1, b.getErrorMessage());
    (void)body_store.finish();
    return body_store.openForRead();
}

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
            return Result<void>(ERROR, "internal fd write failed");
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
            return Result<bool>(ERROR, "internal fd read failed");
        if (n == 0)
            return false;
        buf_.append(buf, static_cast<size_t>(n));
        return true;
    }

    int fd_;
    std::string buf_;
};

static void cleanupDestinationFile_(const std::string& path)
{
    if (path.empty())
        return;
    (void)std::remove(path.c_str());
}

static bool fileExists_(const std::string& path)
{
    struct stat st;
    return (::stat(path.c_str(), &st) == 0);
}

static std::string trimAsciiOws_(const std::string& s)
{
    std::string::size_type b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t'))
        ++b;
    std::string::size_type e = s.size();
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t'))
        --e;
    return s.substr(b, e - b);
}

static bool startsWithNoCase_(const std::string& s, const char* prefix)
{
    const std::string p(prefix);
    if (s.size() < p.size())
        return false;
    for (size_t i = 0; i < p.size(); ++i)
    {
        const unsigned char a = static_cast<unsigned char>(s[i]);
        const unsigned char b = static_cast<unsigned char>(p[i]);
        if (std::tolower(a) != std::tolower(b))
            return false;
    }
    return true;
}

static std::string headerValueAfterColon_(const std::string& line)
{
    std::string::size_type pos = line.find(':');
    if (pos == std::string::npos)
        return std::string();
    return trimAsciiOws_(line.substr(pos + 1));
}

static std::string stripPathComponents_(const std::string& filename)
{
    std::string::size_type p1 = filename.find_last_of('/');
    std::string::size_type p2 = filename.find_last_of('\\');
    std::string::size_type p = std::string::npos;
    if (p1 != std::string::npos)
        p = p1;
    if (p2 != std::string::npos)
        p = (p == std::string::npos) ? p2 : std::max(p, p2);
    return (p == std::string::npos) ? filename : filename.substr(p + 1);
}

static void splitStemAndExt_(
    const std::string& name, std::string* out_stem, std::string* out_ext)
{
    if (out_stem)
        *out_stem = name;
    if (out_ext)
        out_ext->clear();

    const std::string base = stripPathComponents_(name);
    std::string::size_type dot = base.find_last_of('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= base.size())
    {
        if (out_stem)
            *out_stem = base;
        return;
    }
    if (out_stem)
        *out_stem = base.substr(0, dot);
    if (out_ext)
        *out_ext = base.substr(dot + 1);
}

static bool parseContentDispositionFilename_(const std::string& line,
    bool* has_filename_param, std::string* out_filename)
{
    if (has_filename_param)
        *has_filename_param = false;
    if (out_filename)
        out_filename->clear();

    if (!startsWithNoCase_(line, "Content-Disposition:"))
        return false;

    std::string v = headerValueAfterColon_(line);
    std::string lower = toLowerAscii_(v);
    std::string::size_type p = lower.find("filename=");
    if (p == std::string::npos)
        return true;

    if (has_filename_param)
        *has_filename_param = true;

    std::string raw = v.substr(p + std::string("filename=").size());
    raw = trimAsciiOws_(raw);
    if (raw.empty())
        return true;
    if (raw[0] == '"')
    {
        std::string::size_type end = raw.find('"', 1);
        if (end == std::string::npos)
            return true;
        raw = raw.substr(1, end - 1);
    }
    else
    {
        std::string::size_type semi = raw.find(';');
        if (semi != std::string::npos)
            raw = raw.substr(0, semi);
        raw = trimAsciiOws_(raw);
    }

    if (out_filename)
        *out_filename = stripPathComponents_(raw);
    return true;
}

static std::string joinPath_(const std::string& dir, const std::string& leaf)
{
    if (dir.empty())
        return leaf;
    if (!leaf.empty() && leaf[0] == '/')
        return dir + leaf;
    if (dir[dir.size() - 1] == '/')
        return dir + leaf;
    return dir + "/" + leaf;
}

static std::string buildUniqueLeafName_(const std::string& dest_dir,
    const std::string& stem, const std::string& ext)
{
    const std::string safe_stem = stem.empty() ? "file" : stem;
    const std::string safe_ext = ext.empty() ? "bin" : ext;

    std::string leaf = safe_stem + "." + safe_ext;
    std::string full = joinPath_(dest_dir, leaf);
    if (!fileExists_(full))
        return leaf;

    for (int i = 0; i < 1000000; ++i)
    {
        std::ostringstream oss;
        oss << safe_stem << "_" << i << "." << safe_ext;
        leaf = oss.str();
        full = joinPath_(dest_dir, leaf);
        if (!fileExists_(full))
            return leaf;
    }
    return safe_stem + "_" + utils::Timestamp::nowYmdHmsCompact() + "." +
           safe_ext;
}

static Result<void> copyFdToFd_(int in_fd, int out_fd)
{
    char buf[8192];
    for (;;)
    {
        const ssize_t n = ::read(in_fd, buf, sizeof(buf));
        if (n < 0)
            return Result<void>(ERROR, "internal fd read failed");
        if (n == 0)
            break;
        Result<void> w =
            writeAll_(out_fd, std::string(buf, static_cast<size_t>(n)));
        if (w.isError())
            return w;
    }
    return Result<void>();
}

struct FilePartMeta
{
    bool has_filename_param;
    std::string filename;
    bool has_content_type;
    std::string content_type_value;

    FilePartMeta()
        : has_filename_param(false),
          filename(),
          has_content_type(false),
          content_type_value()
    {
    }
};

static Result<void> scanMultipartFileParts_(
    int in_fd, const std::string& boundary, std::vector<FilePartMeta>* out)
{
    if (out == NULL)
        return Result<void>(ERROR, "null out");
    out->clear();

    MultipartStream ms(in_fd);
    const std::string boundary_line = "--" + boundary;
    const std::string boundary_line_final = boundary_line + "--";

    bool found_first_boundary = false;
    for (;;)
    {
        std::string line;
        Result<bool> lr = ms.readLine(&line);
        if (lr.isError())
            return Result<void>(ERROR, lr.getErrorMessage());
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
        return Result<void>(ERROR, "multipart boundary not found");

    for (;;)
    {
        FilePartMeta meta;
        for (;;)
        {
            std::string line;
            Result<bool> lr = ms.readLine(&line);
            if (lr.isError())
                return Result<void>(ERROR, lr.getErrorMessage());
            if (!lr.unwrap())
                return Result<void>(ERROR, "unexpected EOF");
            if (line.empty())
                break;

            bool has_fn = false;
            std::string fn;
            (void)parseContentDispositionFilename_(line, &has_fn, &fn);
            if (has_fn)
            {
                meta.has_filename_param = true;
                meta.filename = fn;
            }
            if (startsWithNoCase_(line, "Content-Type:"))
            {
                meta.has_content_type = true;
                meta.content_type_value = headerValueAfterColon_(line);
            }
        }

        const bool is_file = (meta.has_filename_param || meta.has_content_type);
        bool is_final = false;
        Result<bool> dr = ms.readUntilBoundary(boundary, -1, &is_final);
        if (dr.isError())
            return Result<void>(ERROR, dr.getErrorMessage());
        if (!dr.unwrap())
            return Result<void>(ERROR, "unexpected EOF");
        if (is_file)
            out->push_back(meta);
        if (is_final)
            break;
    }
    return Result<void>();
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
        if (fu.getErrorMessage() == "forbidden")
            return new SendErrorAction(http::HttpStatus::FORBIDDEN);
        if (fu.getErrorMessage() == "internal fd read failed" ||
            fu.getErrorMessage() == "internal fd write failed")
            return new SendErrorAction(http::HttpStatus::SERVER_ERROR);
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

    // upload_store の書き込み可否は事前に access() で判定する。
    // NOTE: TOCTTOU はあるが、HTTPステータス判断の安定化を優先する。
    if (!uctx.destination_dir.empty())
    {
        if (!canWriteToDir_(uctx.destination_dir.str()))
            return Result<void>(ERROR, "forbidden");
    }

    // 1) multipart/form-data: file part を抽出して保存（RFC3875ではなくHTML
    // form upload）
    if (ctx.request.getContentType() == http::ContentType::MULTIPART_FORM_DATA)
    {
        const std::string boundary =
            ctx.request.getContentTypeParam("boundary");
        if (boundary.empty())
            return Result<void>(ERROR, "missing multipart boundary");

        (void)ctx.request_handler.bodyStore().finish();
        Result<int> scan_fd_r = ctx.request_handler.bodyStore().openForRead();
        if (scan_fd_r.isError())
            return Result<void>(ERROR, scan_fd_r.getErrorMessage());
        int scan_fd = scan_fd_r.unwrap();

        std::vector<FilePartMeta> metas;
        Result<void> scanned =
            scanMultipartFileParts_(scan_fd, boundary, &metas);
        ::close(scan_fd);
        if (scanned.isError())
            return Result<void>(ERROR, scanned.getErrorMessage());
        if (metas.empty())
            return Result<void>(ERROR, "no file part found");

        Result<int> in_fd_r = ctx.request_handler.bodyStore().openForRead();
        if (in_fd_r.isError())
            return Result<void>(ERROR, in_fd_r.getErrorMessage());
        int in_fd = in_fd_r.unwrap();

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
                ::close(in_fd);
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
            ::close(in_fd);
            return Result<void>(ERROR, "multipart boundary not found");
        }

        std::string req_stem;
        std::string req_ext;
        splitStemAndExt_(uctx.request_leaf_name, &req_stem, &req_ext);

        size_t file_index = 0;
        for (;;)
        {
            FilePartMeta meta;
            for (;;)
            {
                std::string line;
                Result<bool> lr = ms.readLine(&line);
                if (lr.isError())
                {
                    ::close(in_fd);
                    return Result<void>(ERROR, lr.getErrorMessage());
                }
                if (!lr.unwrap())
                {
                    ::close(in_fd);
                    return Result<void>(ERROR, "unexpected EOF");
                }
                if (line.empty())
                    break;
                bool has_fn = false;
                std::string fn;
                (void)parseContentDispositionFilename_(line, &has_fn, &fn);
                if (has_fn)
                {
                    meta.has_filename_param = true;
                    meta.filename = fn;
                }
                if (startsWithNoCase_(line, "Content-Type:"))
                {
                    meta.has_content_type = true;
                    meta.content_type_value = headerValueAfterColon_(line);
                }
            }

            const bool is_file =
                (meta.has_filename_param || meta.has_content_type);
            int out_fd = -1;
            std::string out_path;
            if (is_file)
            {
                if (file_index >= metas.size())
                {
                    ::close(in_fd);
                    return Result<void>(ERROR, "multipart scan mismatch");
                }
                const FilePartMeta& m = metas[file_index++];

                http::ContentType part_ct = http::ContentType::UNKNOWN;
                std::string part_ext = "bin";
                if (m.has_content_type)
                {
                    part_ct = http::ContentType::parseHeaderValue(
                        m.content_type_value, NULL);
                    if (part_ct != http::ContentType::UNKNOWN)
                        part_ext = http::ContentType(part_ct).toExtention();
                }

                std::string stem;
                if (!m.filename.empty())
                {
                    splitStemAndExt_(m.filename, &stem, NULL);
                }
                else
                {
                    // filename が無い場合のフォールバック
                    if (metas.size() == 1 &&
                        !uctx.request_target_is_directory &&
                        !uctx.request_leaf_name.empty())
                    {
                        stem = req_stem;
                        if (!m.has_content_type && !req_ext.empty())
                            part_ext = req_ext;
                    }
                    else
                    {
                        stem =
                            utils::Timestamp::nowYmdHmsCompact() + "_uploaded";
                        if (!m.has_content_type)
                            part_ext = "bin";
                    }
                }
                if (stem.empty())
                    stem = utils::Timestamp::nowYmdHmsCompact() + "_uploaded";

                const std::string leaf = buildUniqueLeafName_(
                    uctx.destination_dir.str(), stem, part_ext);
                out_path = joinPath_(uctx.destination_dir.str(), leaf);
                out_fd =
                    ::open(out_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
                if (out_fd < 0)
                {
                    ::close(in_fd);
                    return Result<void>(ERROR, "open() failed");
                }
            }

            bool is_final = false;
            Result<bool> dr = ms.readUntilBoundary(boundary, out_fd, &is_final);
            if (out_fd >= 0)
                ::close(out_fd);
            if (dr.isError())
            {
                cleanupDestinationFile_(out_path);
                ::close(in_fd);
                return Result<void>(ERROR, dr.getErrorMessage());
            }
            if (!dr.unwrap())
            {
                cleanupDestinationFile_(out_path);
                ::close(in_fd);
                return Result<void>(ERROR, "unexpected EOF");
            }
            if (is_final)
                break;
        }

        ::close(in_fd);
        return Result<void>();
    }

    // 2) raw / Content-Type 無し / multipart 以外:
    // URL がディレクトリ指定なら、一時ファイルから timestamp 名へ保存する。
    if (uctx.request_target_is_directory)
    {
        (void)ctx.request_handler.bodyStore().finish();
        Result<int> in_fd_r =
            openBodyStoreForReadAllowEmpty_(ctx.request_handler.bodyStore());
        if (in_fd_r.isError())
            return Result<void>(ERROR, in_fd_r.getErrorMessage());
        int in_fd = in_fd_r.unwrap();

        const std::string stem =
            utils::Timestamp::nowYmdHmsCompact() + "_uploaded";
        const std::string leaf =
            buildUniqueLeafName_(uctx.destination_dir.str(), stem, "bin");
        const std::string out_path =
            joinPath_(uctx.destination_dir.str(), leaf);
        const int out_fd =
            ::open(out_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (out_fd < 0)
        {
            ::close(in_fd);
            return Result<void>(ERROR, "open() failed");
        }
        Result<void> c = copyFdToFd_(in_fd, out_fd);
        ::close(out_fd);
        ::close(in_fd);
        if (c.isError())
        {
            cleanupDestinationFile_(out_path);
            return Result<void>(ERROR, c.getErrorMessage());
        }
        return Result<void>();
    }

    return Result<void>();
}

}  // namespace server
