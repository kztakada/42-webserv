#include "server/http_processing_module/request_router/location_routing.hpp"

#include <sys/stat.h>

#include "utils/path.hpp"

namespace server
{
using utils::result::Result;

AutoIndexContext::AutoIndexContext()
    : directory_path(),
      uri_dir_path(),
      index_candidates(),
      autoindex_enabled(false)
{
}

CgiContext::CgiContext()
    : executor_path(),
      script_filename(),
      script_name(),
      path_info(),
      query_string(),
      http_minor_version(0)
{
}

UploadContext::UploadContext()
    : store_root(),
      target_uri_path(),
      destination_path(),
      allow_create_leaf(false),
      allow_overwrite(false)
{
}

LocationRouting::LocationRouting()
    : virtual_server_(NULL),
      location_(NULL),
      request_ctx_(),
      request_method_(http::HttpMethod::UNKNOWN),
      content_length_(0),
      has_content_length_(false),
      status_(http::HttpStatus::SERVER_ERROR)
{
    next_action_ = RESPOND_ERROR;
}

LocationRouting::LocationRouting(const VirtualServer* vserver,
    const LocationDirective* loc, const ResolvedRequestContext& request_ctx,
    const http::HttpRequest& req, http::HttpStatus status)
    : virtual_server_(vserver),
      location_(loc),
      request_ctx_(request_ctx),
      request_method_(req.getMethod()),
      content_length_(0),
      has_content_length_(false),
      status_(status)
{
    (void)decideAction_(req);
}

ActionType LocationRouting::getNextAction() const { return next_action_; }

http::HttpStatus LocationRouting::getHttpStatus() const { return status_; }

Result<std::string> LocationRouting::getRedirectLocation() const
{
    if (next_action_ != REDIRECT_EXTERNAL && next_action_ != REDIRECT_INTERNAL)
    {
        return Result<std::string>(
            utils::result::ERROR, std::string(), "redirect is not selected");
    }
    return redirect_location_;
}

Result<std::string> LocationRouting::getStaticUriPath() const
{
    Result<void> v = validateActionIs_(SERVE_STATIC, "getStaticUriPath");
    if (v.isError())
    {
        return Result<std::string>(
            utils::result::ERROR, std::string(), v.getErrorMessage());
    }
    return request_ctx_.getRequestPath();
}

Result<http::HttpRequest> LocationRouting::getInternalRedirectRequest() const
{
    Result<void> v =
        validateActionIs_(REDIRECT_INTERNAL, "getInternalRedirectRequest");
    if (v.isError())
    {
        return Result<http::HttpRequest>(
            utils::result::ERROR, http::HttpRequest(), v.getErrorMessage());
    }
    return buildInternalRedirectRequest_(redirect_location_);
}

Result<AutoIndexContext> LocationRouting::getAutoIndexContext() const
{
    Result<void> v =
        validateActionIsOneOfStaticOrAutoindex_("getAutoIndexContext");
    if (v.isError())
    {
        return Result<AutoIndexContext>(
            utils::result::ERROR, AutoIndexContext(), v.getErrorMessage());
    }
    if (!location_)
    {
        return Result<AutoIndexContext>(
            utils::result::ERROR, AutoIndexContext(), "no location matched");
    }

    const std::string& uri = request_ctx_.getRequestPath();
    if (uri.empty() || uri[uri.size() - 1] != '/')
    {
        return Result<AutoIndexContext>(utils::result::ERROR,
            AutoIndexContext(), "uri is not a directory path");
    }

    std::string uri_under_location = location_->removePathPatternFromPath(uri);
    if (uri_under_location.empty())
    {
        uri_under_location = "/";
    }
    else if (uri_under_location[0] != '/')
    {
        uri_under_location = "/" + uri_under_location;
    }

    Result<utils::path::PhysicalPath> dir_physical =
        utils::path::resolvePhysicalPathUnderRoot(
            location_->rootDir(), uri_under_location, false);
    if (dir_physical.isError())
    {
        return Result<AutoIndexContext>(utils::result::ERROR,
            AutoIndexContext(), dir_physical.getErrorMessage());
    }

    AutoIndexContext ctx;
    ctx.directory_path = dir_physical.unwrap();
    ctx.uri_dir_path = uri;
    ctx.autoindex_enabled = location_->isAutoIndexEnabled();

    std::vector<std::string> cands = location_->buildIndexCandidatePaths(uri);
    ctx.index_candidates.reserve(cands.size());
    for (size_t i = 0; i < cands.size(); ++i)
    {
        Result<utils::path::PhysicalPath> p =
            utils::path::PhysicalPath::resolve(cands[i]);
        if (p.isError())
        {
            return Result<AutoIndexContext>(utils::result::ERROR,
                AutoIndexContext(), "failed to resolve index candidate");
        }
        ctx.index_candidates.push_back(p.unwrap());
    }
    return ctx;
}

Result<CgiContext> LocationRouting::getCgiContext() const
{
    Result<void> v = validateActionIs_(RUN_CGI, "getCgiContext");
    if (v.isError())
    {
        return Result<CgiContext>(
            utils::result::ERROR, CgiContext(), v.getErrorMessage());
    }
    if (!location_)
    {
        return Result<CgiContext>(
            utils::result::ERROR, CgiContext(), "no location matched");
    }

    const std::string& uri_path = request_ctx_.getRequestPath();
    std::string ext;
    size_t best_end = std::string::npos;
    utils::path::PhysicalPath executor;

    location_->chooseCgiExecutorByRequestPath(
        uri_path, &executor, &ext, &best_end);
    if (best_end == std::string::npos)
    {
        return Result<CgiContext>(
            utils::result::ERROR, CgiContext(), "cgi extension not matched");
    }

    CgiContext ctx;
    ctx.executor_path = executor;
    ctx.script_name = uri_path.substr(0, best_end);
    ctx.path_info = (best_end < uri_path.size()) ? uri_path.substr(best_end)
                                                 : std::string();
    ctx.query_string = query_string_;
    ctx.http_minor_version = request_ctx_.getMinorVersion();

    std::string script_under_location =
        location_->removePathPatternFromPath(ctx.script_name);
    if (script_under_location.empty())
    {
        script_under_location = "/";
    }
    else if (script_under_location[0] != '/')
    {
        script_under_location = "/" + script_under_location;
    }

    Result<utils::path::PhysicalPath> script_physical =
        utils::path::resolvePhysicalPathUnderRoot(
            location_->rootDir(), script_under_location, true);
    if (script_physical.isError())
    {
        return Result<CgiContext>(utils::result::ERROR, CgiContext(),
            script_physical.getErrorMessage());
    }
    ctx.script_filename = script_physical.unwrap();
    return ctx;
}

Result<UploadContext> LocationRouting::getUploadContext() const
{
    Result<void> v = validateActionIs_(STORE_BODY, "getUploadContext");
    if (v.isError())
    {
        return Result<UploadContext>(
            utils::result::ERROR, UploadContext(), v.getErrorMessage());
    }
    if (!location_)
    {
        return Result<UploadContext>(
            utils::result::ERROR, UploadContext(), "no location matched");
    }
    if (!location_->hasUploadStore())
    {
        return Result<UploadContext>(
            utils::result::ERROR, UploadContext(), "upload_store is not set");
    }

    UploadContext ctx;
    ctx.store_root = location_->uploadStore();
    ctx.target_uri_path = request_ctx_.getRequestPath();

    std::string rel =
        location_->removePathPatternFromPath(request_ctx_.getRequestPath());
    if (rel.empty() || rel[rel.size() - 1] == '/')
    {
        return Result<UploadContext>(
            utils::result::ERROR, UploadContext(), "upload target is invalid");
    }
    if (rel[0] != '/')
    {
        rel = "/" + rel;
    }

    Result<utils::path::PhysicalPath> dest =
        utils::path::resolvePhysicalPathUnderRoot(ctx.store_root, rel, true);
    if (dest.isError())
    {
        return Result<UploadContext>(
            utils::result::ERROR, UploadContext(), dest.getErrorMessage());
    }
    ctx.destination_path = dest.unwrap();
    ctx.allow_create_leaf = true;
    // 仕様変更: 同名ファイルがある場合は上書きする
    ctx.allow_overwrite = true;
    return ctx;
}

Result<unsigned long> LocationRouting::clientMaxBodySize() const
{
    if (!location_)
    {
        return Result<unsigned long>(
            utils::result::ERROR, 0, "no location matched");
    }
    return location_->clientMaxBodySize();
}

bool LocationRouting::tryGetErrorPagePath(
    const http::HttpStatus& status, std::string* out_path) const
{
    if (out_path)
        out_path->clear();
    if (location_ && location_->tryGetErrorPagePath(status, out_path))
        return true;
    if (virtual_server_)
        return virtual_server_->tryGetErrorPagePath(status, out_path);
    return false;
}

Result<utils::path::PhysicalPath>
LocationRouting::resolvePhysicalPathUnderRootOrError(
    bool allow_nonexistent_leaf) const
{
    if (status_.isError())
    {
        return Result<utils::path::PhysicalPath>(utils::result::ERROR,
            utils::path::PhysicalPath(), "routing status is error");
    }
    if (!location_)
    {
        return Result<utils::path::PhysicalPath>(utils::result::ERROR,
            utils::path::PhysicalPath(), "no location matched");
    }

    std::string uri_path =
        location_->removePathPatternFromPath(request_ctx_.getRequestPath());
    if (uri_path.empty())
    {
        uri_path = "/";
    }
    else if (uri_path[0] != '/')
    {
        uri_path = "/" + uri_path;
    }

    return utils::path::resolvePhysicalPathUnderRoot(
        location_->rootDir(), uri_path, allow_nonexistent_leaf);
}

Result<utils::path::PhysicalPath>
LocationRouting::resolvePhysicalPathUnderRootOrError() const
{
    return resolvePhysicalPathUnderRootOrError(false);
}

Result<void> LocationRouting::decideAction_(const http::HttpRequest& req)
{
    next_action_ = RESPOND_ERROR;
    redirect_location_.clear();
    query_string_ = req.getQueryString();

    if (status_.isError())
    {
        return applyErrorPageOrRespondError_();
    }
    if (!location_)
    {
        status_ = http::HttpStatus::NOT_FOUND;
        next_action_ = RESPOND_ERROR;
        return applyErrorPageOrRespondError_();
    }

    if (location_->hasRedirect())
    {
        status_ = location_->redirectStatus();
        redirect_location_ = location_->redirectTarget();
        if (!redirect_location_.empty() && redirect_location_[0] == '/')
        {
            next_action_ = REDIRECT_INTERNAL;
        }
        else
        {
            next_action_ = REDIRECT_EXTERNAL;
        }
        return Result<void>();
    }

    // client_max_body_size: Content-Length がある場合だけ先に判定する。
    // chunked/不明は Session 側で受信中に enforce する想定。
    unsigned long max_body = location_->clientMaxBodySize();
    if (req.hasHeader("Content-Length"))
    {
        Result<const std::vector<std::string>&> h =
            req.getHeader("Content-Length");
        if (h.isOk() && !h.unwrap().empty())
        {
            std::istringstream iss(h.unwrap()[0]);
            unsigned long len = 0;
            iss >> len;
            if (!iss.fail())
            {
                has_content_length_ = true;
                content_length_ = len;
                if (len > max_body)
                {
                    status_ = http::HttpStatus::PAYLOAD_TOO_LARGE;
                    return applyErrorPageOrRespondError_();
                }
            }
        }
    }

    // allow methods
    if (!location_->isMethodAllowed(req.getMethod()))
    {
        status_ = http::HttpStatus::NOT_ALLOWED;
        return applyErrorPageOrRespondError_();
    }

    // upload
    // 仕様: upload_store は POST のみ対応
    if (location_->hasUploadStore() &&
        (request_method_ == http::HttpMethod::POST))
    {
        next_action_ = STORE_BODY;
        return Result<void>();
    }

    // autoindex (directory URI + autoindex enabled + no index candidates)
    const std::string& uri = request_ctx_.getRequestPath();
    if (!uri.empty() && uri[uri.size() - 1] == '/')
    {
        std::vector<std::string> cands =
            location_->buildIndexCandidatePaths(uri);
        if (location_->isAutoIndexEnabled() && cands.empty())
        {
            next_action_ = SERVE_AUTOINDEX;
            return Result<void>();
        }
    }

    // CGI (拡張子マッチ)
    if (location_->isCgiEnabled())
    {
        utils::path::PhysicalPath exec;
        std::string ext;
        size_t end = std::string::npos;
        location_->chooseCgiExecutorByRequestPath(
            request_ctx_.getRequestPath(), &exec, &ext, &end);
        if (end != std::string::npos)
        {
            // script の実体がなければ 404（CGI
            // 実行エラーではなくリソース未存在）
            const std::string uri_path = request_ctx_.getRequestPath();
            const std::string script_name = uri_path.substr(0, end);

            std::string script_under_location =
                location_->removePathPatternFromPath(script_name);
            if (script_under_location.empty())
            {
                script_under_location = "/";
            }
            else if (script_under_location[0] != '/')
            {
                script_under_location = "/" + script_under_location;
            }

            Result<utils::path::PhysicalPath> script_physical =
                utils::path::resolvePhysicalPathUnderRoot(
                    location_->rootDir(), script_under_location, true);
            if (script_physical.isError())
            {
                status_ = http::HttpStatus::BAD_REQUEST;
                return applyErrorPageOrRespondError_();
            }

            struct stat st;
            if (::stat(script_physical.unwrap().str().c_str(), &st) != 0 ||
                !S_ISREG(st.st_mode))
            {
                status_ = http::HttpStatus::NOT_FOUND;
                return applyErrorPageOrRespondError_();
            }

            next_action_ = RUN_CGI;
            return Result<void>();
        }
    }

    next_action_ = SERVE_STATIC;
    return Result<void>();
}

Result<void> LocationRouting::applyErrorPageOrRespondError_()
{
    if (!status_.isError())
    {
        next_action_ = SERVE_STATIC;
        return Result<void>();
    }

    next_action_ = RESPOND_ERROR;
    redirect_location_.clear();

    std::string target;
    if (!tryGetErrorPagePath(status_, &target))
    {
        return Result<void>();
    }
    if (!target.empty() && target[0] == '/')
    {
        redirect_location_ = target;
        next_action_ = REDIRECT_INTERNAL;
        return Result<void>();
    }
    // 外部URLエラーリダイレクトは採用しない（RESPOND_ERRORにフォールバック）
    return Result<void>();
}

Result<void> LocationRouting::validateActionIs_(
    ActionType expected, const std::string& api_name) const
{
    if (next_action_ != expected)
    {
        return Result<void>(utils::result::ERROR,
            api_name + " is only valid for a different action");
    }
    return Result<void>();
}

Result<void> LocationRouting::validateActionIsOneOfStaticOrAutoindex_(
    const std::string& api_name) const
{
    if (next_action_ != SERVE_STATIC && next_action_ != SERVE_AUTOINDEX)
    {
        return Result<void>(utils::result::ERROR,
            api_name + " is only valid for SERVE_STATIC/SERVE_AUTOINDEX");
    }
    return Result<void>();
}

Result<http::HttpRequest> LocationRouting::buildInternalRedirectRequest_(
    const std::string& uri_path) const
{
    http::HttpRequest req;
    std::string raw;
    raw += "GET ";
    raw += uri_path;
    raw += " HTTP/1.";
    raw += static_cast<char>('0' + request_ctx_.getMinorVersion());
    raw += "\r\n";
    if (!request_ctx_.getHost().empty())
    {
        raw += "Host: ";
        raw += request_ctx_.getHost();
        raw += "\r\n";
    }
    raw += "\r\n";

    std::vector<utils::Byte> buf;
    buf.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
    {
        buf.push_back(static_cast<utils::Byte>(raw[i]));
    }
    Result<size_t> parsed = req.parse(buf);
    if (parsed.isError() || !req.isParseComplete())
    {
        return Result<http::HttpRequest>(utils::result::ERROR,
            http::HttpRequest(), "failed to build internal redirect request");
    }
    return req;
}

}  // namespace server
