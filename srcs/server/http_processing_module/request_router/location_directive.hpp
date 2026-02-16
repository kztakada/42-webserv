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
    http::HttpStatus redirectStatus() const;

    bool hasUploadStore() const;
    const FilePath& uploadStore() const;

    bool tryGetErrorPagePath(
        const http::HttpStatus& status, std::string* out_path) const;

    // cgi_extensions のマッチング（FSを見ずに request path から判断する）。
    // - out_script_end: script_name の終端位置（uri_path.substr(0, end) が
    // script_name）
    // - 見つからない場合は out_script_end に npos をセットする。
    void chooseCgiExecutorByRequestPath(const std::string& uri_path,
        FilePath* out_executor, std::string* out_ext,
        size_t* out_script_end) const;

    std::vector<std::string> buildIndexCandidatePaths(
        const std::string& request_path) const;

    // request_path がディレクトリURI("/", "/dir/" 等) のときの
    // index 候補 URI パスを返す（buildIndexCandidatePaths と同じ index_pages
    // 順）。
    std::vector<std::string> buildIndexCandidateUriPaths(
        const std::string& request_path) const;

    // ビジネスロジック
    bool isMethodAllowed(const http::HttpMethod& method) const;
    // RFC 9110 Section 10.2.6 (405) の Allow ヘッダー値を生成する。
    // allow_methods で設定されたメソッドを ", " で連結する。
    std::string buildAllowHeaderValue() const;
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

    // root の継承/上書きに関係なく、location のパターンを除去する。
    // 主に upload_store の保存先を「location 配下の相対パス」として
    // 解決したい用途で使う。
    std::string stripPathPatternFromPath(const std::string& path) const;

    // 互換用（呼び出し側移行までの暫定）
    bool isAllowed(const http::HttpMethod& method) const;

   private:
};

}  // namespace server

#endif
