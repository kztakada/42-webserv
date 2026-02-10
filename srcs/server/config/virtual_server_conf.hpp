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
using utils::result::Result;

struct Listen
{
    IPAddress host_ip;
    PortType port;

    Listen() : host_ip(IPAddress::ipv4Any()), port() {}
    Listen(const IPAddress& host, const PortType& p) : host_ip(host), port(p) {}
};

// 仮想サーバーの設定. Nginx の server ブロックに相当.
struct VirtualServerConf
{
    std::vector<Listen> listens;
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
    Result<void> appendListen(
        const std::string& listen_ip_str, const std::string& listen_port_str);
    Result<void> appendListen(const Listen& listen);
    Result<void> appendServerName(const std::string& server_name_str);
    Result<void> setRootDir(const std::string& root_dir_str);
    Result<void> appendIndexPage(const std::string& filename_str);
    Result<void> setClientMaxBodySize(unsigned long size);
    Result<void> appendErrorPage(
        http::HttpStatus status, const std::string& page_url_str);
    Result<void> appendLocation(const LocationDirectiveConf& location);

    // 起動時に bind() する listen のリストを返す（重複解決済み）
    // - 完全同一 (IP:port) は 1つに畳み込む
    // - ある port に 0.0.0.0 が存在する場合、その port の他の IP は含めない
    std::vector<Listen> getListens() const;

    bool isValid() const;
};

}  // namespace server

#endif
