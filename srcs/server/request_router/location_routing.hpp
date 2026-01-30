#ifndef WEBSERV_SERVER_LOCATION_ROUTING_HPP_
#define WEBSERV_SERVER_LOCATION_ROUTING_HPP_

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "http/http_request.hpp"
#include "http/status.hpp"
#include "server/request_router/location_directive.hpp"
#include "server/request_router/virtual_server.hpp"
#include "utils/byte.hpp"
#include "utils/result.hpp"

namespace server
{

class LocationRouting
{
   public:
    LocationRouting();

    LocationRouting(const VirtualServer* vserver, const LocationDirective* loc,
        const std::string& request_path, const std::string& host,
        int minor_version, http::HttpStatus status);

    bool hasVirtualServer() const;
    bool hasLocationDirective() const;

    http::HttpStatus getHttpStatus() const;
    const std::string& getRequestPath() const;

    // 現状は「location.root + request_path(必要ならprefix除去)」の解決のみ。
    // ファイルの存在確認(stat)等は上位で行う想定。
    utils::result::Result<std::string> getPath() const;

    // Session層でファイルアクセス直前に呼ぶ想定。
    // - location.root を root_dir として、request_path から location pattern
    // を除去したパスを辿る。
    // - symlink による root 逸脱を検知した場合は ERROR。
    // - allow_nonexistent_leaf=true の場合、末尾が存在しなくても親が安全なら
    // OK。
    utils::result::Result<utils::path::PhysicalPath>
    resolvePhysicalPathUnderRootOrError(bool allow_nonexistent_leaf) const;
    utils::result::Result<utils::path::PhysicalPath>
    resolvePhysicalPathUnderRootOrError() const;

    // error_page が外部リダイレクト（http/https）のときだけ OK。
    // 内部リダイレクト（'/'）のときは ERROR。
    utils::result::Result<std::string> getErrorPagePath(
        const http::HttpStatus& status) const;

    utils::result::Result<http::HttpRequest> getErrorRequest(
        const http::HttpStatus& status) const;

    static std::string getDefaultErrorPage(const http::HttpStatus& status);

   private:
    static std::string toString_(const http::HttpStatus& status);

    const VirtualServer* virtual_server_;
    const LocationDirective* location_;
    std::string request_path_;
    std::string host_;
    int minor_version_;
    http::HttpStatus status_;

    std::string buildInternalRedirectRequestRaw_(
        const std::string& uri_path) const;
};

}  // namespace server

#endif
