#ifndef UTILS_LOG_HPP_
#define UTILS_LOG_HPP_

#include <fstream>
#include <iostream>
#include <string>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "utils/path.hpp"
#include "utils/timestamp.hpp"

namespace utils
{
class Log
{
   public:
    // プログラムの実行状態を出力
    template <typename T1>
    static void display(const T1& p1)
    {
        log_core(std::cerr, "", p1);
    }
    static void displayListen(const IPAddress& ip, const PortType& port)
    {
        log_core(std::cerr, "Server listening on ",
            ip.toString() + ":" + port.toString());
    }

    enum FileType
    {
        INFO,
        ERROR,
        F_DEBUG
    };
    // errorログをファイルに出力
    template <typename T1, typename T2, typename T3>
    static void error(const T1& p1, const T2& p2, const T3& p3)
    {
        std::ofstream ofs(getFilePath(ERROR), std::ios::app);
        if (ofs)
            log_core(ofs, "[ERROR] ", Timestamp::now(), p1, p2, p3);
        else
            failedToOpenLogFile(ERROR);
    }

    // infoログ出力
    template <typename T1>
    static void info(const T1& p1)
    {
        std::ofstream ofs(getFilePath(INFO), std::ios::app);
        if (ofs)
            log_core(ofs, "[INFO] ", Timestamp::now(), p1);
        else
            failedToOpenLogFile(INFO);
    }
    // debugログ出力
    template <typename T1>
    static void debug(const T1& p1)
    {
#ifdef DEBUG
        std::ofstream ofs(getFilePath(F_DEBUG), std::ios::app);
        if (ofs)
            log_core(ofs, "[DEBUG] ", Timestamp::now(), p1);
        else
            failedToOpenLogFile(F_DEBUG);
#else
        (void)p1;
#endif
    }
    // warningログ出力（可変引数対応）
    template <typename T1>
    static void warning(const T1& p1)
    {
        log_core(std::cerr, "[WARNING] ", p1);
    }
    template <typename T1, typename T2>
    static void warning(const T1& p1, const T2& p2)
    {
        log_core(std::cerr, "[WARNING] ", p1, p2);
    }
    template <typename T1, typename T2, typename T3>
    static void warning(const T1& p1, const T2& p2, const T3& p3)
    {
        log_core(std::cerr, "[WARNING] ", p1, p2, p3);
    }
    template <typename T1, typename T2, typename T3, typename T4>
    static void warning(const T1& p1, const T2& p2, const T3& p3, const T4& p4)
    {
        log_core(std::cerr, "[WARNING] ", p1, p2, p3, p4);
    }
    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    static void warning(
        const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5)
    {
        log_core(std::cerr, "[WARNING] ", p1, p2, p3, p4, p5);
    }

   private:
    Log();
    Log(const Log& other);
    Log& operator=(const Log& other);
    ~Log();

    static const char* getFilePath(FileType type)
    {
        if (type == ERROR)
            return "./logs/error.txt";
        if (type == INFO)
            return "./logs/info.txt";
        if (type == F_DEBUG)
            return "./logs/debug.txt";
        else
            return "./logs/default.txt";
    }

    // 引数1つの実装
    template <typename T1>
    static void log_core(std::ostream& os, const char* label, const T1& p1)
    {
        os << label << p1 << std::endl;
    }

    // 引数2つの実装
    template <typename T1, typename T2>
    static void log_core(
        std::ostream& os, const char* label, const T1& p1, const T2& p2)
    {
        os << label << p1 << " " << p2 << std::endl;
    }

    // 引数3つの実装
    template <typename T1, typename T2, typename T3>
    static void log_core(std::ostream& os, const char* label, const T1& p1,
        const T2& p2, const T3& p3)
    {
        os << label << "(" << p1 << ") " << p2 << " " << p3 << std::endl;
    }

    // 引数4つの実装
    template <typename T1, typename T2, typename T3, typename T4>
    static void log_core(std::ostream& os, const char* label, const T1& p1,
        const T2& p2, const T3& p3, const T4& p4)
    {
        os << label << p1 << " " << p2 << " " << p3 << " " << p4 << std::endl;
    }

    // 引数5つの実装
    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    static void log_core(std::ostream& os, const char* label, const T1& p1,
        const T2& p2, const T3& p3, const T4& p4, const T5& p5)
    {
        os << label << p1 << " " << p2 << " " << p3 << " " << p4 << " " << p5
           << std::endl;
    }

    static void failedToOpenLogFile(FileType type)
    {
        std::cerr << "[ERROR] Failed to open log file." << " at "
                  << getFilePath(type) << std::endl;
    }
};

}  // namespace utils

#endif