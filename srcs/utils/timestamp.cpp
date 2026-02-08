#include "utils/timestamp.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace utils
{
std::string Timestamp::now()
{
    return formatHmsFromEpochSeconds(nowEpochSeconds());
}

long Timestamp::nowEpochSeconds()
{
    const std::time_t now = std::time(NULL);
    return static_cast<long>(now);
}

std::string Timestamp::formatHmsFromEpochSeconds(long epoch_seconds)
{
    const std::time_t t = static_cast<std::time_t>(epoch_seconds);
    std::tm* lt = std::localtime(&t);

    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << lt->tm_hour << ":"
       << std::setw(2) << std::setfill('0') << lt->tm_min << ":" << std::setw(2)
       << std::setfill('0') << lt->tm_sec;
    return ss.str();
}
}  // namespace utils