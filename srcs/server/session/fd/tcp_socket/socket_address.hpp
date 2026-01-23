#ifndef WEBSERV_SOCKET_ADDRESS_HPP_
#define WEBSERV_SOCKET_ADDRESS_HPP_

#include <netinet/in.h>

#include <string>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"

namespace server
{

class SocketAddress
{
   private:
    struct sockaddr_in addr_;
    IPAddress ip_;
    PortType port_;
    std::string name_;

   public:
    SocketAddress();
    explicit SocketAddress(const struct sockaddr_in& addr);
    SocketAddress(const SocketAddress& rhs);
    SocketAddress& operator=(const SocketAddress& rhs);
    ~SocketAddress();

    IPAddress getIp() const;
    PortType getPort() const;
    std::string getName() const;
    const struct sockaddr_in& getAddr() const;
};

}  // namespace server

#endif
