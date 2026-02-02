#ifndef WEBSERV_LOCATION_CONF_HPP_
#define WEBSERV_LOCATION_CONF_HPP_

#include <climits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "http/http_method.hpp"
#include "http/http_request.hpp"
#include "http/status.hpp"
#include "server/value_types.hpp"
#include "utils/result.hpp"

namespace server
{
using utils::result::Result;
// serverディレクティブ内のlocationディレクティブの情報
struct LocationDirectiveConf
{
    typedef std::map<CgiExt, FilePath> CgiExtensionsMap;

    // clang-tidy(performance.Padding) 対応: パディングを減らすため並び順を調整
    unsigned long client_max_body_size;
    std::vector<FileName> index_pages;
    URIPath path_pattern;
    FilePath root_dir;
    TargetURI redirect_url;  // returnディレクティブで指定されたURL
    FilePath upload_store;   // upload_storeディレクティブで指定された保存先
    std::set<http::HttpMethod> allowed_methods;
    CgiExtensionsMap cgi_extensions;
    ErrorPagesMap error_pages;
    http::HttpStatus
        redirect_status;  // returnディレクティブで指定されたステータス
    bool is_backward_search;
    bool has_allowed_methods;
    bool has_client_max_body_size;
    bool has_root_dir;
    bool has_index_pages;
    bool has_error_pages;
    bool auto_index;  // ディレクトリ内ファイル一覧ページを有効にするかどうか
    bool has_auto_index;

    // デフォルト値での初期化
    LocationDirectiveConf();

    // --- 仕様チェック込みの登録API（パーサーから利用する想定） ---
    Result<void> setPathPattern(const std::string& path_pattern_str);
    Result<void> setIsBackwardSearch(bool is_backward_search);
    Result<void> appendAllowedMethod(const http::HttpMethod& method);
    Result<void> setClientMaxBodySize(unsigned long size);
    Result<void> setRootDir(const std::string& root_dir_str);
    Result<void> appendIndexPage(const std::string& filename_str);
    Result<void> appendCgiExtension(
        const std::string& cgi_ext_str, const std::string& cgi_path_str);
    Result<void> appendErrorPage(
        http::HttpStatus status, const std::string& page_url_str);
    Result<void> setAutoIndex(bool auto_index);
    Result<void> setRedirect(
        http::HttpStatus status, const std::string& redirect_url_str);
    Result<void> setUploadStore(const std::string& upload_store_str);

    // バリデーション（データの整合性チェック）
    bool isValid() const;
};

}  // namespace server

#endif