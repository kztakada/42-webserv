#include "server/request_processor/request_processor.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#include <cstddef>
#include <vector>

#include "http/content_types.hpp"
#include "http/header.hpp"

namespace server
{

static std::string extractExtension_(const std::string& path)
{
    const std::string::size_type slash = path.find_last_of('/');
    const std::string::size_type dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return std::string();
    if (slash != std::string::npos && dot < slash)
        return std::string();
    return path.substr(dot);
}

static Result<RequestProcessor::Output> respondWithErrorPage_(
    const http::HttpStatus& status, http::HttpResponse& out_response)
{
    RequestProcessor::Output out;

    Result<void> s = out_response.setStatus(status);
    if (s.isError())
        return Result<RequestProcessor::Output>(ERROR, s.getErrorMessage());

    const std::string body = LocationRouting::getDefaultErrorPageBody(status);
    (void)out_response.setHeader("Content-Type", "text/html");
    (void)out_response.setExpectedContentLength(
        static_cast<unsigned long>(body.size()));

    out.body_source = new StringBodySource(body);
    out.should_close_connection = false;
    return out;
}

static Result<http::HttpRequest> buildInternalGetRequest_(
    const std::string& uri_path, const http::HttpRequest& base)
{
    http::HttpRequest req;
    std::string raw;
    raw += "GET ";
    raw += uri_path;
    raw += " HTTP/1.";
    raw += static_cast<char>('0' + base.getMinorVersion());
    raw += "\r\n";

    // Host は vserver 選択のため可能なら維持
    Result<const std::vector<std::string>&> h = base.getHeader("Host");
    if (h.isOk() && !h.unwrap().empty())
    {
        raw += "Host: ";
        raw += h.unwrap()[0];
        raw += "\r\n";
    }
    raw += "\r\n";

    std::vector<utils::Byte> buf;
    buf.reserve(raw.size());
    for (std::string::size_type i = 0; i < raw.size(); ++i)
    {
        buf.push_back(static_cast<utils::Byte>(raw[i]));
    }

    Result<size_t> parsed = req.parse(buf, NULL, false);
    if (parsed.isError())
    {
        return Result<http::HttpRequest>(
            ERROR, http::HttpRequest(), parsed.getErrorMessage());
    }
    if (!req.isParseComplete())
    {
        return Result<http::HttpRequest>(ERROR, http::HttpRequest(),
            "internal redirect request is not complete");
    }
    return req;
}

static bool tryBuildErrorPageInternalRedirect_(const RequestRouter& router,
    const IPAddress& server_ip, const PortType& server_port,
    const http::HttpRequest& base_request, const http::HttpStatus& error_status,
    http::HttpRequest* out_next)
{
    if (out_next == NULL)
        return false;

    Result<LocationRouting> route_result =
        router.route(base_request, server_ip, server_port);
    if (route_result.isError())
        return false;

    LocationRouting route = route_result.unwrap();
    std::string target;
    if (!route.tryGetErrorPagePath(error_status, &target))
        return false;
    if (target.empty() || target[0] != '/')
        return false;

    // 自己ループ防止（/errors/404.html の解決で同じURIに飛び続けるのを防ぐ）
    if (base_request.getPath() == target)
        return false;

    Result<http::HttpRequest> ir =
        buildInternalGetRequest_(target, base_request);
    if (ir.isError())
        return false;
    *out_next = ir.unwrap();
    return true;
}

RequestProcessor::RequestProcessor(const RequestRouter& router,
    const IPAddress& server_ip, const PortType& server_port)
    : router_(router), server_ip_(server_ip), server_port_(server_port)
{
}

RequestProcessor::~RequestProcessor() {}

Result<RequestProcessor::Output> RequestProcessor::process(
    const http::HttpRequest& request, http::HttpResponse& out_response)
{
    // 内部リダイレクトを最小限サポートする（error_page の内部URI等）
    http::HttpRequest current(request);
    bool has_preserved_error_status = false;
    http::HttpStatus preserved_error_status(http::HttpStatus::OK);
    for (int redirect_guard = 0; redirect_guard < 5; ++redirect_guard)
    {
        Result<LocationRouting> route_result =
            router_.route(current, server_ip_, server_port_);
        if (route_result.isError())
            return Result<Output>(ERROR, route_result.getErrorMessage());

        LocationRouting route = route_result.unwrap();
        const ActionType action = route.getNextAction();

        if (action == REDIRECT_INTERNAL)
        {
            // error_page の内部リダイレクトは「コンテンツだけ差し替えて、
            // 元のエラーステータスは維持する」挙動にする。
            if (!has_preserved_error_status && route.getHttpStatus().isError())
            {
                has_preserved_error_status = true;
                preserved_error_status = route.getHttpStatus();
            }
            Result<http::HttpRequest> r = route.getInternalRedirectRequest();
            if (r.isError())
                return Result<Output>(ERROR, r.getErrorMessage());
            current = r.unwrap();
            continue;
        }

        Output out;

        if (action == REDIRECT_EXTERNAL)
        {
            Result<void> s = out_response.setStatus(route.getHttpStatus());
            if (s.isError())
                return Result<Output>(ERROR, s.getErrorMessage());

            Result<std::string> loc = route.getRedirectLocation();
            if (loc.isError())
                return Result<Output>(ERROR, loc.getErrorMessage());

            Result<void> h = out_response.setHeader("Location", loc.unwrap());
            if (h.isError())
                return Result<Output>(ERROR, h.getErrorMessage());

            (void)out_response.setExpectedContentLength(0);
            out.body_source = NULL;
            out.should_close_connection = false;
            return out;
        }

        if (action == RESPOND_ERROR)
        {
            return respondWithErrorPage_(route.getHttpStatus(), out_response);
        }

        if (action != SERVE_STATIC && action != SERVE_AUTOINDEX)
        {
            return respondWithErrorPage_(
                http::HttpStatus::NOT_IMPLEMENTED, out_response);
        }

        // --- Static / Autoindex ---
        // filesystem を見て最終決定する。

        utils::result::Result<utils::path::PhysicalPath> resolved =
            route.resolvePhysicalPathUnderRootOrError();
        if (resolved.isError())
        {
            http::HttpRequest next;
            if (tryBuildErrorPageInternalRedirect_(router_, server_ip_,
                    server_port_, current, http::HttpStatus::NOT_FOUND, &next))
            {
                if (!has_preserved_error_status)
                {
                    has_preserved_error_status = true;
                    preserved_error_status = http::HttpStatus::NOT_FOUND;
                }
                current = next;
                continue;
            }
            return respondWithErrorPage_(
                http::HttpStatus::NOT_FOUND, out_response);
        }

        std::string target_path = resolved.unwrap().str();

        struct stat st;
        if (::stat(target_path.c_str(), &st) != 0)
        {
            http::HttpRequest next;
            if (tryBuildErrorPageInternalRedirect_(router_, server_ip_,
                    server_port_, current, http::HttpStatus::NOT_FOUND, &next))
            {
                if (!has_preserved_error_status)
                {
                    has_preserved_error_status = true;
                    preserved_error_status = http::HttpStatus::NOT_FOUND;
                }
                current = next;
                continue;
            }
            return respondWithErrorPage_(
                http::HttpStatus::NOT_FOUND, out_response);
        }

        // directory
        if (S_ISDIR(st.st_mode))
        {
            const std::string& uri = current.getPath();
            if (!uri.empty() && uri[uri.size() - 1] != '/')
            {
                // RFC的には末尾スラッシュを付けるためのリダイレクト。
                Result<void> s =
                    out_response.setStatus(http::HttpStatus::MOVED_PERMANENTLY);
                if (s.isError())
                    return Result<Output>(ERROR, s.getErrorMessage());
                Result<void> h = out_response.setHeader("Location", uri + "/");
                if (h.isError())
                    return Result<Output>(ERROR, h.getErrorMessage());
                (void)out_response.setExpectedContentLength(0);
                out.body_source = NULL;
                out.should_close_connection = false;
                return out;
            }

            // index candidates を探す
            Result<AutoIndexContext> ai = route.getAutoIndexContext();
            if (ai.isOk())
            {
                const AutoIndexContext ctx = ai.unwrap();
                for (size_t i = 0; i < ctx.index_candidates.size(); ++i)
                {
                    const std::string cand = ctx.index_candidates[i].str();
                    struct stat st2;
                    if (::stat(cand.c_str(), &st2) != 0)
                        continue;
                    if (!S_ISREG(st2.st_mode))
                        continue;

                    int fd = ::open(cand.c_str(), O_RDONLY);
                    if (fd < 0)
                        continue;

                    unsigned long size =
                        static_cast<unsigned long>(st2.st_size);
                    Result<void> s = out_response.setStatus(
                        has_preserved_error_status
                            ? preserved_error_status
                            : http::HttpStatus(http::HttpStatus::OK));
                    if (s.isError())
                        return Result<Output>(ERROR, s.getErrorMessage());

                    (void)out_response.setExpectedContentLength(size);
                    const std::string ext = extractExtension_(cand);
                    http::ContentType ct =
                        http::ContentType::fromExtension(ext);
                    (void)out_response.setHeader("Content-Type", ct.c_str());

                    out.body_source = new FileBodySource(fd, size);
                    out.should_close_connection = false;
                    return out;
                }
            }

            // autoindex は未実装なので 403 を返す
            http::HttpRequest next;
            if (tryBuildErrorPageInternalRedirect_(router_, server_ip_,
                    server_port_, current, http::HttpStatus::FORBIDDEN, &next))
            {
                if (!has_preserved_error_status)
                {
                    has_preserved_error_status = true;
                    preserved_error_status = http::HttpStatus::FORBIDDEN;
                }
                current = next;
                continue;
            }
            return respondWithErrorPage_(
                http::HttpStatus::FORBIDDEN, out_response);
        }

        if (!S_ISREG(st.st_mode))
        {
            http::HttpRequest next;
            if (tryBuildErrorPageInternalRedirect_(router_, server_ip_,
                    server_port_, current, http::HttpStatus::NOT_FOUND, &next))
            {
                if (!has_preserved_error_status)
                {
                    has_preserved_error_status = true;
                    preserved_error_status = http::HttpStatus::NOT_FOUND;
                }
                current = next;
                continue;
            }
            return respondWithErrorPage_(
                http::HttpStatus::NOT_FOUND, out_response);
        }

        int fd = ::open(target_path.c_str(), O_RDONLY);
        if (fd < 0)
        {
            http::HttpRequest next;
            if (tryBuildErrorPageInternalRedirect_(router_, server_ip_,
                    server_port_, current, http::HttpStatus::NOT_FOUND, &next))
            {
                if (!has_preserved_error_status)
                {
                    has_preserved_error_status = true;
                    preserved_error_status = http::HttpStatus::NOT_FOUND;
                }
                current = next;
                continue;
            }
            return respondWithErrorPage_(
                http::HttpStatus::NOT_FOUND, out_response);
        }

        unsigned long size = static_cast<unsigned long>(st.st_size);
        Result<void> s = out_response.setStatus(
            has_preserved_error_status
                ? preserved_error_status
                : http::HttpStatus(http::HttpStatus::OK));
        if (s.isError())
            return Result<Output>(ERROR, s.getErrorMessage());

        (void)out_response.setExpectedContentLength(size);

        const std::string ext = extractExtension_(target_path);
        http::ContentType ct = http::ContentType::fromExtension(ext);
        (void)out_response.setHeader("Content-Type", ct.c_str());

        out.body_source = new FileBodySource(fd, size);
        out.should_close_connection = false;
        return out;
    }

    return Result<Output>(ERROR, "too many internal redirects");
}

Result<RequestProcessor::Output> RequestProcessor::processError(
    const http::HttpRequest& request, const http::HttpStatus& error_status,
    http::HttpResponse& out_response)
{
    // まずは設定に沿った error_page（内部URI）を探す。
    std::string target;
    Result<LocationRouting> route_result =
        router_.route(request, server_ip_, server_port_);
    if (route_result.isOk())
    {
        LocationRouting route = route_result.unwrap();
        if (route.tryGetErrorPagePath(error_status, &target))
        {
            if (!target.empty() && target[0] == '/')
            {
                Result<http::HttpRequest> ir =
                    buildInternalGetRequest_(target, request);
                if (ir.isOk())
                {
                    out_response.reset();
                    Result<Output> pr = process(ir.unwrap(), out_response);
                    if (pr.isOk() && out_response.getStatus().isSuccess())
                    {
                        // コンテンツは内部URIの結果を使い、ステータスだけ元の
                        // エラーに戻す。
                        (void)out_response.setStatus(error_status);
                        return pr.unwrap();
                    }

                    if (pr.isOk())
                    {
                        Output o = pr.unwrap();
                        if (o.body_source != NULL)
                        {
                            delete o.body_source;
                            o.body_source = NULL;
                        }
                    }
                }
            }
        }
    }

    out_response.reset();
    return respondWithErrorPage_(error_status, out_response);
}

}  // namespace server
