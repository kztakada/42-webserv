#ifndef WEBSERV_SERVER_RESOLVED_REQUEST_CONTEXT_HPP_
#define WEBSERV_SERVER_RESOLVED_REQUEST_CONTEXT_HPP_

#include <string>
#include <vector>

#include "http/http_request.hpp"
#include "utils/result.hpp"

namespace server
{
using utils::result::Result;

class ResolvedRequestContext
{
   public:
    ResolvedRequestContext();

    static Result<ResolvedRequestContext> create(
        const http::HttpRequest& request);

    // create() に失敗した場合でも BAD_REQUEST のレスポンス構築に必要な
    // 情報（minor_version 等）を保持するためのフォールバック。
    static ResolvedRequestContext createForBadRequest(
        const http::HttpRequest& request);

    const std::string& getRequestPath() const;
    const std::string& getHost() const;
    int getMinorVersion() const;

    const http::HttpRequest* getRequest() const;

    // RFC 3986 Section 5.2.4 (Remove Dot Segments) 相当
    // - '.' と '..' を解決する
    // - ルートより上に出ようとしたら ERROR
    Result<void> resolveDotSegmentsOrError();

   private:
    const http::HttpRequest* request_;
    std::string request_path_;
    std::string host_;
    int minor_version_;

    static std::string extractHost_(const http::HttpRequest& request);
    static std::string normalizeSlashes_(const std::string& path);
};

}  // namespace server

#endif
