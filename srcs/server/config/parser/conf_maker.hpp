#ifndef WEBSERV_CONF_MAKER_HPP_
#define WEBSERV_CONF_MAKER_HPP_

#include <string>

#include "http/http_method.hpp"
#include "http/status.hpp"
#include "server/config/location_directive_conf.hpp"
#include "server/config/virtual_server_conf.hpp"
#include "utils/result.hpp"

namespace server
{
using utils::result::Result;

// locationディレクティブの設定を構築するビルダークラス
class LocationDirectiveConfMaker
{
   private:
    LocationDirectiveConf conf_;

   public:
    LocationDirectiveConfMaker() : conf_() {}

    // Setter群（パース用）
    Result<void> setPathPattern(const std::string& path_pattern)
    {
        return conf_.setPathPattern(path_pattern);
    }

    Result<void> setIsBackwardSearch(bool is_backward_search)
    {
        return conf_.setIsBackwardSearch(is_backward_search);
    }

    Result<void> appendAllowedMethod(const http::HttpMethod& method)
    {
        return conf_.appendAllowedMethod(method);
    }

    Result<void> setClientMaxBodySize(unsigned long size)
    {
        return conf_.setClientMaxBodySize(size);
    }

    Result<void> setRootDir(const std::string& root_dir)
    {
        return conf_.setRootDir(root_dir);
    }

    Result<void> appendIndexPage(const FileName& filename)
    {
        return conf_.appendIndexPage(filename);
    }

    Result<void> appendCgiExtension(
        const std::string& extension, const std::string& executor_path)
    {
        return conf_.appendCgiExtension(extension, executor_path);
    }

    Result<void> appendErrorPage(
        http::HttpStatus status, const TargetURI& page_url)
    {
        return conf_.appendErrorPage(status, page_url);
    }

    Result<void> setAutoIndex(bool auto_index)
    {
        return conf_.setAutoIndex(auto_index);
    }

    Result<void> setRedirect(
        http::HttpStatus status, const std::string& redirect_url)
    {
        return conf_.setRedirect(status, redirect_url);
    }

    Result<void> setUploadStore(const std::string& upload_store)
    {
        return conf_.setUploadStore(upload_store);
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

    Result<void> appendListen(
        const std::string& listen_ip, const std::string& listen_port)
    {
        return conf_.appendListen(listen_ip, listen_port);
    }

    Result<void> appendServerName(const std::string& server_name)
    {
        return conf_.appendServerName(server_name);
    }

    Result<void> setRootDir(const std::string& root_dir)
    {
        return conf_.setRootDir(root_dir);
    }

    Result<void> appendIndexPage(const FileName& filename)
    {
        return conf_.appendIndexPage(filename);
    }

    Result<void> setClientMaxBodySize(unsigned long size)
    {
        return conf_.setClientMaxBodySize(size);
    }

    Result<void> appendErrorPage(
        http::HttpStatus status, const TargetURI& page_url)
    {
        return conf_.appendErrorPage(status, page_url);
    }

    Result<void> appendLocation(const LocationDirectiveConf& location)
    {
        return conf_.appendLocation(location);
    }

    VirtualServerConf build() const { return conf_; }

    bool isValid() const { return conf_.isValid(); }
};

}  // namespace server

#endif