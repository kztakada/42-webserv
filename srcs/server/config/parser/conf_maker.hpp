#ifndef WEBSERV_CONF_MAKER_HPP_
#define WEBSERV_CONF_MAKER_HPP_

#include <string>

#include "http/http_method.hpp"
#include "http/status.hpp"
#include "server/config/location_directive_conf.hpp"
#include "server/config/virtual_server_conf.hpp"

namespace server
{

// locationディレクティブの設定を構築するビルダークラス
class LocationDirectiveConfMaker
{
   private:
    LocationDirectiveConf conf_;

   public:
    LocationDirectiveConfMaker() : conf_() {}

    // Setter群（パース用）
    void setPathPattern(const std::string& path_pattern)
    {
        conf_.path_pattern = path_pattern;
    }

    void setIsBackwardSearch(bool is_backward_search)
    {
        conf_.is_backward_search = is_backward_search;
    }

    void appendAllowedMethod(const http::HttpMethod& method)
    {
        conf_.allowed_methods.insert(method);
    }

    void setClientMaxBodySize(unsigned long size)
    {
        conf_.client_max_body_size = size;
    }

    void setRootDir(const std::string& root_dir) { conf_.root_dir = root_dir; }

    void appendIndexPage(const LocationDirectiveConf::PagePath& filepath)
    {
        conf_.index_pages.push_back(filepath);
    }

    void setIsCgi(bool is_cgi) { conf_.is_cgi = is_cgi; }

    void setCgiExecutor(const std::string& cgi_executor)
    {
        conf_.cgi_executor = cgi_executor;
    }

    void appendErrorPage(http::HttpStatus status,
        const LocationDirectiveConf::PagePath& filepath)
    {
        conf_.error_pages[status] = filepath;
    }

    void setAutoIndex(bool auto_index) { conf_.auto_index = auto_index; }

    void setRedirectUrl(const std::string& redirect_url)
    {
        conf_.redirect_url = redirect_url;
    }

    // 構築完了（データを返す）
    LocationDirectiveConf build() const { return conf_; }

    // バリデーション（Build前のチェック）
    bool isValid() const { return conf_.isValid(); }
};

// virtual_serverの設定を構築するビルダークラス
class VirtualServerConfMaker
{
   private:
    VirtualServerConf conf_;

   public:
    VirtualServerConfMaker() : conf_() {}

    void setListenIp(const std::string& listen_ip)
    {
        conf_.listen_ip = listen_ip;
    }

    void setListenPort(const PortType& listen_port)
    {
        conf_.listen_port = listen_port;
    }

    void appendServerName(const std::string& server_name)
    {
        conf_.server_names.insert(server_name);
    }

    void appendLocation(const LocationDirectiveConf& location)
    {  // ← LocationConf（データ）
        conf_.locations.push_back(location);
    }

    VirtualServerConf build() const { return conf_; }

    bool isValid() const { return conf_.isValid(); }
};

}  // namespace server

#endif