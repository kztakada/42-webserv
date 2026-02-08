#ifndef UTILS_TIMESTAMP_HPP_
#define UTILS_TIMESTAMP_HPP_

#include <string>

namespace utils
{
class Timestamp
{
   public:
    static std::string now();

   private:
    Timestamp();
    Timestamp(const Timestamp& other);
    Timestamp& operator=(const Timestamp& other);
    ~Timestamp();
};
}  // namespace utils
#endif