#include "server/session/fd/tcp_socket/socket_address.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <sstream>

namespace server
{

SocketAddress::SocketAddress() { std::memset(&addr_, 0, sizeof(addr_)); }

SocketAddress::SocketAddress(const struct sockaddr_in& addr) : addr_(addr)
{
    // IPアドレスの変換
    ip_ = IPAddress(addr);

    // ポート番号の変換
    port_ = PortType(addr);

    // ホスト名（今は空）
    name_ = "";
}

SocketAddress::SocketAddress(const SocketAddress& rhs)
    : addr_(rhs.addr_), ip_(rhs.ip_), port_(rhs.port_), name_(rhs.name_)
{
}

SocketAddress& SocketAddress::operator=(const SocketAddress& rhs)
{
    if (this != &rhs)
    {
        addr_ = rhs.addr_;
        ip_ = rhs.ip_;
        port_ = rhs.port_;
        name_ = rhs.name_;
    }
    return *this;
}

SocketAddress::~SocketAddress() {}

IPAddress SocketAddress::getIp() const { return ip_; }

PortType SocketAddress::getPort() const { return port_; }

std::string SocketAddress::getName() const { return name_; }

const struct sockaddr_in& SocketAddress::getAddr() const { return addr_; }

}  // namespace server
