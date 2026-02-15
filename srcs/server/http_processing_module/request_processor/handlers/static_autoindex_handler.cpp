#include "server/http_processing_module/request_processor/handlers/static_autoindex_handler.hpp"

#include <sys/stat.h>

#include <cerrno>
#include <cstdio>

#include "server/http_processing_module/request_processor/autoindex_renderer.hpp"
#include "server/http_processing_module/request_processor/error_page_renderer.hpp"
#include "server/http_processing_module/request_processor/internal_redirect_resolver.hpp"
#include "server/http_processing_module/request_processor/static_file_responder.hpp"
#include "server/session/fd_session/http_session/body_source.hpp"

namespace server
{

using utils::result::Result;

static bool tryInternalRedirect_(const InternalRedirectResolver& resolver,
    const RequestRouter& router, const IPAddress& server_ip,
    const PortType& server_port, const http::HttpRequest& current,
    const http::HttpStatus& error_status, ProcessingState* state,
    http::HttpRequest* out_next)
{
    if (!resolver.tryBuildErrorPageInternalRedirect(
            router, server_ip, server_port, current, error_status, out_next))
    {
        return false;
    }
    if (state != NULL)
    {
        if (!state->has_preserved_error_status)
        {
            state->has_preserved_error_status = true;
            state->preserved_error_status = error_status;
        }
    }
    return true;
}

Result<HandlerResult> StaticAutoIndexHandler::handle(
    const LocationRouting& route, const IPAddress& server_ip,
    const PortType& server_port, http::HttpResponse& out_response,
    ProcessingState* state)
{
    if (state == NULL)
        return Result<HandlerResult>(ERROR, "state is null");

    // --- Static / Autoindex ---
    // filesystem を見て最終決定する。

    Result<utils::path::PhysicalPath> resolved =
        route.resolvePhysicalPathUnderRootOrError();
    if (resolved.isError())
    {
        http::HttpRequest next;
        if (tryInternalRedirect_(internal_redirect_, router_, server_ip,
                server_port, state->current, http::HttpStatus::NOT_FOUND, state,
                &next))
        {
            HandlerResult res;
            res.should_continue = true;
            res.next_request = next;
            return res;
        }

        Result<RequestProcessorOutput> r =
            error_renderer_.respond(http::HttpStatus::NOT_FOUND, out_response);
        if (r.isError())
            return Result<HandlerResult>(ERROR, r.getErrorMessage());
        HandlerResult res;
        res.output = r.unwrap();
        return res;
    }

    std::string target_path = resolved.unwrap().str();

    struct stat st;
    if (::stat(target_path.c_str(), &st) != 0)
    {
        http::HttpRequest next;
        if (tryInternalRedirect_(internal_redirect_, router_, server_ip,
                server_port, state->current, http::HttpStatus::NOT_FOUND, state,
                &next))
        {
            HandlerResult res;
            res.should_continue = true;
            res.next_request = next;
            return res;
        }

        Result<RequestProcessorOutput> r =
            error_renderer_.respond(http::HttpStatus::NOT_FOUND, out_response);
        if (r.isError())
            return Result<HandlerResult>(ERROR, r.getErrorMessage());
        HandlerResult res;
        res.output = r.unwrap();
        return res;
    }

    // DELETE: method が許可されている場合のみここに到達する（405 は routing
    // 側）。 優先順: 404 -> 403
    if (state->current.getMethod() == http::HttpMethod::DELETE)
    {
        if (S_ISDIR(st.st_mode))
        {
            http::HttpRequest next;
            if (tryInternalRedirect_(internal_redirect_, router_, server_ip,
                    server_port, state->current, http::HttpStatus::FORBIDDEN,
                    state, &next))
            {
                HandlerResult res;
                res.should_continue = true;
                res.next_request = next;
                return res;
            }

            Result<RequestProcessorOutput> r = error_renderer_.respond(
                http::HttpStatus::FORBIDDEN, out_response);
            if (r.isError())
                return Result<HandlerResult>(ERROR, r.getErrorMessage());
            HandlerResult res;
            res.output = r.unwrap();
            return res;
        }

        if (!S_ISREG(st.st_mode))
        {
            http::HttpRequest next;
            if (tryInternalRedirect_(internal_redirect_, router_, server_ip,
                    server_port, state->current, http::HttpStatus::NOT_FOUND,
                    state, &next))
            {
                HandlerResult res;
                res.should_continue = true;
                res.next_request = next;
                return res;
            }

            Result<RequestProcessorOutput> r = error_renderer_.respond(
                http::HttpStatus::NOT_FOUND, out_response);
            if (r.isError())
                return Result<HandlerResult>(ERROR, r.getErrorMessage());
            HandlerResult res;
            res.output = r.unwrap();
            return res;
        }

        errno = 0;
        const int rc = std::remove(target_path.c_str());
        if (rc == 0)
        {
            // success
            Result<void> s =
                out_response.setStatus(http::HttpStatus::NO_CONTENT);
            if (s.isError())
                return Result<HandlerResult>(ERROR, s.getErrorMessage());

            HandlerResult res;
            (void)out_response.setExpectedContentLength(0);
            res.output.body_source.reset(NULL);
            res.output.should_close_connection = false;
            return res;
        }

        const http::HttpStatus err = (errno == ENOENT)
                                         ? http::HttpStatus::NOT_FOUND
                                         : http::HttpStatus::FORBIDDEN;
        http::HttpRequest next;
        if (tryInternalRedirect_(internal_redirect_, router_, server_ip,
                server_port, state->current, err, state, &next))
        {
            HandlerResult res;
            res.should_continue = true;
            res.next_request = next;
            return res;
        }

        Result<RequestProcessorOutput> r =
            error_renderer_.respond(err, out_response);
        if (r.isError())
            return Result<HandlerResult>(ERROR, r.getErrorMessage());
        HandlerResult res;
        res.output = r.unwrap();
        return res;
    }

    // directory
    if (S_ISDIR(st.st_mode))
    {
        const std::string& uri = state->current.getPath();
        const bool has_trailing_slash =
            (!uri.empty() && uri[uri.size() - 1] == '/');

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

                Result<RequestProcessorOutput> rf =
                    file_responder_.respondFile(cand, st2,
                        state->has_preserved_error_status
                            ? state->preserved_error_status
                            : http::HttpStatus(http::HttpStatus::OK),
                        out_response);
                if (rf.isOk())
                {
                    HandlerResult res;
                    res.output = rf.unwrap();
                    return res;
                }

                if (rf.getErrorMessage() == "forbidden")
                {
                    // index ファイルが存在するが読めない場合は 403。
                    http::HttpRequest next;
                    if (tryInternalRedirect_(internal_redirect_, router_,
                            server_ip, server_port, state->current,
                            http::HttpStatus::FORBIDDEN, state, &next))
                    {
                        HandlerResult res;
                        res.should_continue = true;
                        res.next_request = next;
                        return res;
                    }

                    Result<RequestProcessorOutput> r = error_renderer_.respond(
                        http::HttpStatus::FORBIDDEN, out_response);
                    if (r.isError())
                        return Result<HandlerResult>(
                            ERROR, r.getErrorMessage());
                    HandlerResult res;
                    res.output = r.unwrap();
                    return res;
                }
            }

            if (ctx.autoindex_enabled)
            {
                Result<std::string> body = autoindex_renderer_.buildBody(ctx);
                if (body.isError())
                {
                    http::HttpRequest next;
                    if (tryInternalRedirect_(internal_redirect_, router_,
                            server_ip, server_port, state->current,
                            http::HttpStatus::FORBIDDEN, state, &next))
                    {
                        HandlerResult res;
                        res.should_continue = true;
                        res.next_request = next;
                        return res;
                    }

                    Result<RequestProcessorOutput> r = error_renderer_.respond(
                        http::HttpStatus::FORBIDDEN, out_response);
                    if (r.isError())
                        return Result<HandlerResult>(
                            ERROR, r.getErrorMessage());
                    HandlerResult res;
                    res.output = r.unwrap();
                    return res;
                }

                Result<void> s = out_response.setStatus(
                    state->has_preserved_error_status
                        ? state->preserved_error_status
                        : http::HttpStatus(http::HttpStatus::OK));
                if (s.isError())
                    return Result<HandlerResult>(ERROR, s.getErrorMessage());

                (void)out_response.setHeader("Content-Type", "text/html");
                (void)out_response.setExpectedContentLength(
                    static_cast<unsigned long>(body.unwrap().size()));

                HandlerResult res;
                res.output.body_source.reset(
                    new StringBodySource(body.unwrap()));
                res.output.should_close_connection = false;
                return res;
            }
        }

        // index が無く、autoindex も無効の場合は 403。
        // ただし末尾'/'なしの要求は「ファイルとしての要求」とみなし、
        // ディレクトリに index が無い場合は 404 を返す（42_test の期待）。
        const http::HttpStatus final_status = has_trailing_slash
                                                  ? http::HttpStatus::FORBIDDEN
                                                  : http::HttpStatus::NOT_FOUND;
        http::HttpRequest next;
        if (tryInternalRedirect_(internal_redirect_, router_, server_ip,
                server_port, state->current, final_status, state, &next))
        {
            HandlerResult res;
            res.should_continue = true;
            res.next_request = next;
            return res;
        }

        Result<RequestProcessorOutput> r =
            error_renderer_.respond(final_status, out_response);
        if (r.isError())
            return Result<HandlerResult>(ERROR, r.getErrorMessage());
        HandlerResult res;
        res.output = r.unwrap();
        return res;
    }

    if (!S_ISREG(st.st_mode))
    {
        http::HttpRequest next;
        if (tryInternalRedirect_(internal_redirect_, router_, server_ip,
                server_port, state->current, http::HttpStatus::NOT_FOUND, state,
                &next))
        {
            HandlerResult res;
            res.should_continue = true;
            res.next_request = next;
            return res;
        }

        Result<RequestProcessorOutput> r =
            error_renderer_.respond(http::HttpStatus::NOT_FOUND, out_response);
        if (r.isError())
            return Result<HandlerResult>(ERROR, r.getErrorMessage());
        HandlerResult res;
        res.output = r.unwrap();
        return res;
    }

    Result<RequestProcessorOutput> rf =
        file_responder_.respondFile(target_path, st,
            state->has_preserved_error_status
                ? state->preserved_error_status
                : http::HttpStatus(http::HttpStatus::OK),
            out_response);
    if (rf.isError())
    {
        http::HttpStatus err = http::HttpStatus::NOT_FOUND;
        if (rf.getErrorMessage() == "forbidden")
            err = http::HttpStatus::FORBIDDEN;
        else if (rf.getErrorMessage() != "not_found")
            err = http::HttpStatus::SERVER_ERROR;

        http::HttpRequest next;
        if (tryInternalRedirect_(internal_redirect_, router_, server_ip,
                server_port, state->current, err, state, &next))
        {
            HandlerResult res;
            res.should_continue = true;
            res.next_request = next;
            return res;
        }

        Result<RequestProcessorOutput> r =
            error_renderer_.respond(err, out_response);
        if (r.isError())
            return Result<HandlerResult>(ERROR, r.getErrorMessage());
        HandlerResult res;
        res.output = r.unwrap();
        return res;
    }

    HandlerResult res;
    res.output = rf.unwrap();
    return res;
}

}  // namespace server
