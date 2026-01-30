#include "network/ip_address.hpp"

#include <arpa/inet.h>

#include <sstream>

namespace
{

utils::result::Result<IPAddress> makeInvalidIpResult()
{
    return utils::result::Result<IPAddress>(
        utils::result::ERROR, "ip address is invalid");
}

}  // namespace

IPAddress::IPAddress() : ip_str_("") {}

IPAddress::IPAddress(const sockaddr_in& addr)
{
    // sockaddr_inからIPv4アドレス文字列への変換
    // 禁止関数 inet_ntop を使わず、ntohl + 文字列化で生成する
    const unsigned long host_order = ntohl(addr.sin_addr.s_addr);
    const unsigned int b1 = (host_order >> 24) & 0xFF;
    const unsigned int b2 = (host_order >> 16) & 0xFF;
    const unsigned int b3 = (host_order >> 8) & 0xFF;
    const unsigned int b4 = (host_order) & 0xFF;

    std::ostringstream oss;
    oss << b1 << '.' << b2 << '.' << b3 << '.' << b4;
    ip_str_ = oss.str();
}

IPAddress IPAddress::ipv4Any() { return IPAddress("0.0.0.0"); }

utils::result::Result<IPAddress> IPAddress::parseIpv4Numeric(
    const std::string& ip_str)
{
    if (ip_str.empty())
    {
        return utils::result::Result<IPAddress>(
            utils::result::ERROR, "ip address is empty");
    }

    unsigned int octets[4];
    unsigned int octet_index = 0;
    unsigned int current = 0;
    bool has_digit = false;
    unsigned int digit_count = 0;

    for (size_t i = 0; i < ip_str.size(); ++i)
    {
        const char c = ip_str[i];
        if (c >= '0' && c <= '9')
        {
            has_digit = true;
            ++digit_count;
            if (digit_count > 3)
            {
                return makeInvalidIpResult();
            }
            current = current * 10u + static_cast<unsigned int>(c - '0');
            if (current > 255u)
            {
                return makeInvalidIpResult();
            }
        }
        else if (c == '.')
        {
            if (!has_digit)
            {
                return makeInvalidIpResult();
            }
            if (octet_index >= 4)
            {
                return makeInvalidIpResult();
            }
            octets[octet_index] = current;
            ++octet_index;

            current = 0;
            has_digit = false;
            digit_count = 0;
        }
        else
        {
            return makeInvalidIpResult();
        }
    }

    if (!has_digit)
    {
        return makeInvalidIpResult();
    }
    if (octet_index != 3)
    {
        return makeInvalidIpResult();
    }
    octets[3] = current;

    std::ostringstream oss;
    oss << octets[0] << '.' << octets[1] << '.' << octets[2] << '.'
        << octets[3];
    return IPAddress(oss.str());
}

const std::string& IPAddress::toString() const { return ip_str_; }

const bool IPAddress::empty() const { return ip_str_.empty(); }
