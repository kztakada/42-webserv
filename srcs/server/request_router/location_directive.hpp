#ifndef WEBSERV_SERVER_LOCATION_DIRECTIVE_HPP_
#define WEBSERV_SERVER_LOCATION_DIRECTIVE_HPP_

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "http/http_method.hpp"
#include "server/config/location_directive_conf.hpp"

namespace server
{

class LocationDirective
{
   private:
    LocationDirectiveConf conf_;

    static bool forwardMatch_(
        const std::string& path, const std::string& pattern);
    static bool backwardMatch_(
        const std::string& path, const std::string& pattern);
    static std::string joinPath_(
        const std::string& base, const std::string& leaf);

   public:
    // Configからの変換用コンストラクタ
    explicit LocationDirective(const LocationDirectiveConf& conf);

    // 問い合わせ/解決API（外部にConfig構造を露出しない）
    unsigned long clientMaxBodySize() const;
    const FilePath& rootDir() const;
    bool isBackwardSearch() const;
    bool isCgiEnabled() const;
    bool isAutoIndexEnabled() const;
    bool hasRedirect() const;
    const std::string& redirectTarget() const;

    bool tryGetErrorPagePath(
        const http::HttpStatus& status, std::string* out_path) const;

    std::vector<std::string> buildIndexCandidatePaths(
        const std::string& request_path) const;

    // ビジネスロジック
    bool isMethodAllowed(const http::HttpMethod& method) const;
    size_t pathPatternLength() const;
    bool isMatchPattern(const std::string& path) const;

    // location : /cgi-bin
    // root dir : /public/cgi-bin
    // req      : /cgi-bin/test-cgi;
    // -> /public/cgi-bin/test-cgi
    std::string getAbsolutePath(const std::string& path) const;

    // path     : /cgi-bin/test-cgi/hoge/fuga
    // location : /cgi-bin
    // -> /test-cgi/hoge/fuga
    std::string removePathPatternFromPath(const std::string& path) const;

    // 互換用（呼び出し側移行までの暫定）
    bool isAllowed(const http::HttpMethod& method) const;

   private:
};

}  // namespace server

#endif