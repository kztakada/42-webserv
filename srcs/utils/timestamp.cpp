#include "utils/timestamp.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace utils
{
std::string Timestamp::now()
{
    std::time_t now = std::time(NULL);

    // ローカル時刻の構造体に変換
    std::tm* lt = std::localtime(&now);

    std::stringstream ss;
    ss << lt->tm_year + 1900 << "/" << std::setw(2) << std::setfill('0')
       << lt->tm_mon + 1 << "/" << std::setw(2) << std::setfill('0')
       << lt->tm_mday << " " << std::setw(2) << std::setfill('0') << lt->tm_hour
       << ":" << std::setw(2) << std::setfill('0') << lt->tm_min << ":"
       << std::setw(2) << std::setfill('0') << lt->tm_sec;
    return ss.str();
}
}  // namespace utils