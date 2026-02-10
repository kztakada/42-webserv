#ifndef WEBSERV_SERVER_PORT_TYPE_HPP_
#define WEBSERV_SERVER_PORT_TYPE_HPP_

#include <arpa/inet.h>

#include <string>

#include "utils/result.hpp"

using namespace utils::result;

// getaddrinfo()などの標準ライブラリのインターフェースに合わせている｡
// サービス名("http")などは受け付けず､数値での指定のみ
class PortType
{
   public:
    PortType();
    PortType(const sockaddr_in& addr);

    // サービス名("http")などは受け付けず､数値での指定のみ
    static Result<PortType> parseNumeric(const std::string& port_str);

    unsigned int toNumber() const;
    const std::string& toString() const;
    bool empty() const;

   private:
    explicit PortType(const std::string& port_str);

    std::string port_str_;
};

#endif
