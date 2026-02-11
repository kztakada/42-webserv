#ifndef UTILS_TIMESTAMP_HPP_
#define UTILS_TIMESTAMP_HPP_

#include <ctime>
#include <string>

namespace utils
{
class Timestamp
{
   public:
    // 現在時刻の文字列表現（HH:MM:SS）
    static std::string now();

    // 秒単位のエポック時刻（計測用途）
    static long nowEpochSeconds();

    // エポック秒を HH:MM:SS に整形
    static std::string formatHmsFromEpochSeconds(long epoch_seconds);

    // 現在時刻を YYYYMMDDHHMMSS で返す（アップロードファイル名用途）。
    static std::string nowYmdHmsCompact();

   private:
    Timestamp();
    Timestamp(const Timestamp& other);
    Timestamp& operator=(const Timestamp& other);
    ~Timestamp();
};
}  // namespace utils
#endif
