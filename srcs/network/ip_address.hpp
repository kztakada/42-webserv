#ifndef WEBSERV_SERVER_IP_ADDRESS_HPP_
#define WEBSERV_SERVER_IP_ADDRESS_HPP_

#include <netinet/in.h>

#include <string>

#include "utils/result.hpp"

class IPAddress
{
   public:
    IPAddress();
    IPAddress(const sockaddr_in& addr);

    static IPAddress ipv4Any();

    // 数値IPv4の文字列表現を検証してIPAddressを生成する。
    // 例: "127.0.0.1" はOK、"localhost" や "999.0.0.1" はERROR。
    static utils::result::Result<IPAddress> parseIpv4Numeric(
        const std::string& ip_str);

    const std::string& toString() const;
    const bool empty() const;

   private:
    explicit IPAddress(const std::string& ip_str) : ip_str_(ip_str) {}

    std::string ip_str_;
};

#endif