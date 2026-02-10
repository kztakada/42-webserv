#ifndef WEBSERV_SERVER_IP_ADDRESS_HPP_
#define WEBSERV_SERVER_IP_ADDRESS_HPP_

#include <netinet/in.h>

#include <string>

#include "utils/result.hpp"

using namespace utils::result;

class IPAddress
{
   public:
    IPAddress();
    IPAddress(const sockaddr_in& addr);

    static IPAddress ipv4Any();

    // 数値IPv4の文字列表現を検証してIPAddressを生成する。
    // 例: "127.0.0.1" はOK、"localhost" や "999.0.0.1" はERROR。
    static Result<IPAddress> parseIpv4Numeric(const std::string& ip_str);

    bool isWildcard() const;

    const std::string& toString() const;
    bool empty() const;

   private:
    explicit IPAddress(const std::string& ip_str)
        : ip_str_(ip_str), is_wildcard_(ip_str == "0.0.0.0")
    {
    }

    std::string ip_str_;
    bool is_wildcard_;
};

#endif
