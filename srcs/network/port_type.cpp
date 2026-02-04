#include "network/port_type.hpp"

#include <sstream>

#include "utils/result.hpp"

using namespace utils::result;

PortType::PortType() : port_str_("") {}

PortType::PortType(const sockaddr_in& addr)
{
    std::ostringstream oss;
    oss << ntohs(addr.sin_port);
    port_str_ = oss.str();
}

PortType::PortType(const std::string& port_str) : port_str_(port_str) {}

Result<PortType> PortType::parseNumeric(const std::string& port_str)
{
    if (port_str.empty())
        return Result<PortType>(ERROR, "port is empty");

    unsigned long value = 0;
    for (size_t i = 0; i < port_str.size(); ++i)
    {
        const char c = port_str[i];
        if (c < '0' || c > '9')
            return Result<PortType>(ERROR, "port must be numeric");

        value = value * 10ul + static_cast<unsigned long>(c - '0');
        if (value > 65535ul)
            return Result<PortType>(ERROR, "port is out of range");
    }
    if (value == 0ul)
        return Result<PortType>(ERROR, "port is out of range");

    std::ostringstream oss;
    oss << value;
    return PortType(oss.str());
}

const std::string& PortType::toString() const { return port_str_; }

unsigned int PortType::toNumber() const
{
    if (port_str_.empty())
        return 0;

    unsigned long value = 0;
    for (size_t i = 0; i < port_str_.size(); ++i)
    {
        const char c = port_str_[i];
        if (c < '0' || c > '9')
            return 0;
        value = value * 10ul + static_cast<unsigned long>(c - '0');
        if (value > 65535ul)
            return 0;
    }
    return static_cast<unsigned int>(value);
}

bool PortType::empty() const { return port_str_.empty(); }
