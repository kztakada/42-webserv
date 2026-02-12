#ifndef WEBSERV_SERVER_LOCATION_ROUTING_HPP_
#define WEBSERV_SERVER_LOCATION_ROUTING_HPP_

#include <cstddef>
#include <string>
#include <vector>

#include "http/http_request.hpp"
#include "http/status.hpp"
#include "server/http_processing_module/request_router/location_directive.hpp"
#include "server/http_processing_module/request_router/resolved_request_context.hpp"
#include "server/http_processing_module/request_router/virtual_server.hpp"
#include "utils/byte.hpp"
#include "utils/path.hpp"
#include "utils/result.hpp"

namespace server
{
using namespace utils::result;

enum ActionType
{
    SERVE_STATIC,
    SERVE_AUTOINDEX,
    RUN_CGI,
    STORE_BODY,
    REDIRECT_EXTERNAL,
    REDIRECT_INTERNAL,
    RESPOND_ERROR
};

struct AutoIndexContext
{
    utils::path::PhysicalPath directory_path;
    std::string uri_dir_path;
    std::vector<utils::path::PhysicalPath> index_candidates;
    bool autoindex_enabled;

    AutoIndexContext();
};

struct CgiContext
{
    utils::path::PhysicalPath executor_path;
    utils::path::PhysicalPath script_filename;
    std::string script_name;
    std::string path_info;
    std::string query_string;
    int http_minor_version;

    CgiContext();
};

struct UploadContext
{
    utils::path::PhysicalPath store_root;
    std::string target_uri_path;
    // アップロード保存先ディレクトリ（upload_store 配下で解決された物理パス）
    utils::path::PhysicalPath destination_dir;
    // リクエストURIがファイル指定だった場合の leaf 名（例: "test.txt"）。
    // ディレクトリ指定("/upload/" 等)の場合は空。
    std::string request_leaf_name;
    bool request_target_is_directory;
    bool allow_create_leaf;
    bool allow_overwrite;

    UploadContext();
};

class LocationRouting
{
   public:
    LocationRouting();

    LocationRouting(const VirtualServer* vserver, const LocationDirective* loc,
        const ResolvedRequestContext& request_ctx, const http::HttpRequest& req,
        http::HttpStatus status);

    ActionType getNextAction() const;
    http::HttpStatus getHttpStatus() const;

    // 405 Method Not Allowed の Allow ヘッダー値（"GET, POST" 等）。
    // location が存在しない場合は ERROR。
    Result<std::string> getAllowHeaderValue() const;

    // REDIRECT_EXTERNAL のときは URL、REDIRECT_INTERNAL のときは URI。
    Result<std::string> getRedirectLocation() const;

    // SERVE_STATIC の「次に扱うURI」。
    Result<std::string> getStaticUriPath() const;

    // REDIRECT_INTERNAL の場合に、次の内部リクエストを生成して返す。
    Result<http::HttpRequest> getInternalRedirectRequest() const;

    // SERVE_STATIC / SERVE_AUTOINDEX の場合のみ有効。
    // uri がディレクトリを指す場合の index 候補や autoindex 方針を返す。
    Result<AutoIndexContext> getAutoIndexContext() const;

    // RUN_CGI の場合のみ有効。
    Result<CgiContext> getCgiContext() const;

    // STORE_BODY の場合のみ有効。
    Result<UploadContext> getUploadContext() const;

    Result<unsigned long> clientMaxBodySize() const;

    // error_page の設定を問い合わせる（設定がなければ false）。
    // out_path はそのまま location 設定の値（URIパス or URL）を返す。
    bool tryGetErrorPagePath(
        const http::HttpStatus& status, std::string* out_path) const;

    // Session層でファイルアクセス直前に呼ぶ想定。
    // - location.root を root_dir として、request_path から location pattern
    // を除去したパスを辿る。
    // - symlink による root 逸脱を検知した場合は ERROR。
    // - allow_nonexistent_leaf=true の場合、末尾が存在しなくても親が安全なら
    // OK。
    Result<utils::path::PhysicalPath> resolvePhysicalPathUnderRootOrError(
        bool allow_nonexistent_leaf) const;
    Result<utils::path::PhysicalPath> resolvePhysicalPathUnderRootOrError()
        const;

   private:
    const VirtualServer* virtual_server_;
    const LocationDirective* location_;
    ResolvedRequestContext request_ctx_;
    http::HttpMethod request_method_;
    unsigned long content_length_;
    bool has_content_length_;
    http::HttpStatus status_;
    ActionType next_action_;
    std::string redirect_location_;
    std::string query_string_;

    Result<void> decideAction_(const http::HttpRequest& req);
    Result<void> applyErrorPageOrRespondError_();

    Result<void> validateActionIs_(
        ActionType expected, const std::string& api_name) const;
    Result<void> validateActionIsOneOfStaticOrAutoindex_(
        const std::string& api_name) const;

    Result<http::HttpRequest> buildInternalRedirectRequest_(
        const std::string& uri_path) const;
};

}  // namespace server

#endif
