#ifndef WEBSERV_SERVER_PORT_TYPE_HPP_
#define WEBSERV_SERVER_PORT_TYPE_HPP_

#include <arpa/inet.h>

#include <sstream>
#include <string>

// getaddrinfo()などの標準ライブラリのインターフェースに合わせている｡
// サービス名("http")などは受け付けず､数値での指定のみ
class PortType
{
   public:
    PortType() : port_str_("") {}
    PortType(const sockaddr_in& addr)
    {
        // sockaddr_inからポート番号への変換
        std::ostringstream oss;
        oss << ntohs(addr.sin_port);
        port_str_ = oss.str();
    }
    PortType(const std::string& port_str) : port_str_(port_str)
    {
        // 追加のバリデーションが必要ならここに書く
    }
    const std::string& toString() const { return port_str_; }

    const bool empty() const { return port_str_.empty(); }

   private:
    std::string port_str_;
};

#endif