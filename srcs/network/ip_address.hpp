#ifndef WEBSERV_SERVER_IP_ADDRESS_HPP_
#define WEBSERV_SERVER_IP_ADDRESS_HPP_

#include <arpa/inet.h>

#include <string>

class IPAddress
{
   public:
    IPAddress() : ip_str_("") {}
    IPAddress(const sockaddr_in& addr)
    {
        // sockaddr_inからIPアドレスへの変換
        char ip_buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_buf, sizeof(ip_buf));
        ip_str_ = ip_buf;
    }
    IPAddress(const std::string& ip_str) : ip_str_(ip_str)
    {
        // 追加のバリデーションが必要ならここに書く
    }
    const std::string& toString() const { return ip_str_; }
    const bool empty() const { return ip_str_.empty(); }

   private:
    std::string ip_str_;
};

#endif