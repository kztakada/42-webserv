#ifndef WEBSERV_SERVER_RESOLVED_PATH_HPP_
#define WEBSERV_SERVER_RESOLVED_PATH_HPP_

#include <cstddef>
#include <string>
#include <vector>

#include "http/http_request.hpp"
#include "utils/result.hpp"

namespace server
{

class ResolvedPath
{
   public:
    static utils::result::Result<ResolvedPath> create(
        const http::HttpRequest& request);

    const std::string& getPath() const;
    const std::string& getHost() const;
    int getMinorVersion() const;

    // RFC 3986 Section 5.2.4 (Remove Dot Segments) 相当
    // - '.' と '..' を解決する
    // - ルートより上に出ようとしたら ERROR
    utils::result::Result<std::string> resolveDotSegmentsOrError() const;

   private:
    std::string path_;
    std::string host_;
    int minor_version_;

    ResolvedPath();

    static std::string extractHost_(const http::HttpRequest& request);
    static std::string normalizeSlashes_(const std::string& path);
};

}  // namespace server

#endif
