#ifndef WEBSERV_SERVER_LOCATION_DIRECTIVE_HPP_
#define WEBSERV_SERVER_LOCATION_DIRECTIVE_HPP_

#include <set>
#include <string>

#include "http/http_method.hpp"
#include "server/config/location_directive_conf.hpp"

namespace server
{

class LocationDirective
{
   private:
    const LocationDirectiveConf conf_;

   public:
    // Configからの変換用コンストラクタ
    explicit LocationDirective(const LocationDirectiveConf& conf);

    // Getter（データへのアクセス）
    const std::string& getPathPattern() const { return conf_.path_pattern; }
    bool getIsBackwardSearch() const { return conf_.is_backward_search; }
    unsigned long getClientMaxBodySize() const
    {
        return conf_.client_max_body_size;
    }
    const std::string& getRootDir() const { return conf_.root_dir; }
    bool getIsCgi() const { return conf_.is_cgi; }
    std::string getCgiExecutor() const { return conf_.cgi_executor; }
    bool getAutoIndex() const { return conf_.auto_index; }
    const std::vector<LocationDirectiveConf::PagePath>& getIndexPages() const
    {
        return conf_.index_pages;
    }
    const LocationDirectiveConf::ErrorPagesMap& getErrorPages() const
    {
        return conf_.error_pages;
    }
    std::string getRedirectUrl() const { return conf_.redirect_url; }

    // ビジネスロジック
    bool isAllowed(const http::HttpMethod& method) const
    {
        return conf_.allowed_methods.find(method) !=
               conf_.allowed_methods.end();
    }

    bool isMatchPattern(const std::string& path) const;
    // {
    // 	if (conf_.is_backward_search)
    // 	{
    // 		return utils::BackwardMatch(path, conf_.path_pattern);
    // 	}
    // 	else
    // 	{
    // 		return utils::ForwardMatch(path, conf_.path_pattern);
    // 	}
    // }

    // location : /cgi-bin
    // root dir : /public/cgi-bin
    // req      : /cgi-bin/test-cgi;
    // -> /public/cgi-bin/test-cgi
    std::string getAbsolutePath(const std::string& path) const;
    // {
    // 	if (conf_.is_backward_search)
    // 	{
    // 		return utils::JoinPath(conf_.root_dir, path);
    // 	}
    // 	return utils::JoinPath(
    // 		conf_.root_dir,
    // 		path.substr(conf_.path_pattern.length()));
    // }

    // path     : /cgi-bin/test-cgi/hoge/fuga
    // location : /cgi-bin
    // -> /test-cgi/hoge/fuga
    std::string removePathPatternFromPath(const std::string& path) const
    {
        if (conf_.is_backward_search)
        {
            return path;
        }
        return path.substr(conf_.path_pattern.length());
    }

   private:
    // コピー禁止（不要なので）
    LocationDirective(const LocationDirective&);
    LocationDirective& operator=(const LocationDirective&);
};

}  // namespace server

#endif