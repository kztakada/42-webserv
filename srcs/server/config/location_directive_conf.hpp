#ifndef WEBSERV_LOCATION_CONF_HPP_
#define WEBSERV_LOCATION_CONF_HPP_

#include <climits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "http/http_method.hpp"
#include "http/status.hpp"

namespace server
{

// serverディレクティブ内のlocationディレクティブの情報
struct LocationDirectiveConf
{
    typedef std::string PagePath;
    typedef std::map<http::HttpStatus, PagePath> ErrorPagesMap;

    std::string path_pattern;
    bool is_backward_search;
    std::set<http::HttpMethod> allowed_methods;
    unsigned long client_max_body_size;
    std::string root_dir;
    std::vector<PagePath> index_pages;
    bool is_cgi;
    std::string cgi_executor;
    ErrorPagesMap error_pages;
    bool auto_index;  // ディレクトリ内ファイル一覧ページを有効にするかどうか
    std::string redirect_url;  // returnディレクティブで指定されたURL

    // デフォルト値での初期化
    LocationDirectiveConf()
        : is_backward_search(false),
          client_max_body_size(1024 * 1024),  // 1MB
          is_cgi(false),
          auto_index(false)
    {
    }

    // バリデーション（データの整合性チェック）
    bool isValid() const
    {
        if (root_dir.empty() && redirect_url.empty())
        {
            return false;
        }
        if (client_max_body_size > INT_MAX)
        {
            return false;
        }
        if (is_cgi ^ !cgi_executor.empty())
        {
            return false;
        }
        return true;
    }
};

}  // namespace server

#endif