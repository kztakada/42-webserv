#ifndef WEBSERV_VIRTUAL_SERVER_CONF_HPP_
#define WEBSERV_VIRTUAL_SERVER_CONF_HPP_

#include <climits>
#include <set>
#include <string>
#include <vector>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "server/config/location_directive_conf.hpp"
#include "server/value_types.hpp"
#include "utils/result.hpp"

namespace server
{

// 仮想サーバーの設定. Nginx の server ブロックに相当.
struct VirtualServerConf
{
    IPAddress listen_ip;
    PortType listen_port;
    std::set<std::string> server_names;

    // server コンテキストのディレクティブ
    FilePath root_dir;
    std::vector<FileName> index_pages;
    unsigned long client_max_body_size;
    bool has_client_max_body_size;
    ErrorPagesMap error_pages;

    std::vector<LocationDirectiveConf> locations;

    VirtualServerConf();

    // --- 仕様チェック込みの登録API（パーサーから利用する想定） ---
    utils::result::Result<void> setListenIp(const std::string& listen_ip_str);
    utils::result::Result<void> setListenPort(
        const std::string& listen_port_str);
    utils::result::Result<void> appendServerName(
        const std::string& server_name_str);
    utils::result::Result<void> setRootDir(const std::string& root_dir_str);
    utils::result::Result<void> appendIndexPage(
        const std::string& filename_str);
    utils::result::Result<void> setClientMaxBodySize(unsigned long size);
    utils::result::Result<void> appendErrorPage(
        http::HttpStatus status, const std::string& page_url_str);
    utils::result::Result<void> appendLocation(
        const LocationDirectiveConf& location);

    bool isValid() const;
};

}  // namespace server

#endif