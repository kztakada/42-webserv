#ifndef UTILS_LOG_HPP_
#define UTILS_LOG_HPP_

#include <fstream>
#include <iostream>
#include <string>

namespace utils
{
class Log
{
   public:
    // errorログ出力（可変引数対応）
    template <typename T1>
    static void error(const T1& p1)
    {
        std::ofstream ofs(error_file_path_.c_str(), std::ios::app);
        if (ofs)
            log_core(ofs, "[ERROR] ", p1);
        else
            failedToOpenLogFile();
    }
    template <typename T1, typename T2>
    static void error(const T1& p1, const T2& p2)
    {
        std::ofstream ofs(error_file_path_.c_str(), std::ios::app);
        if (ofs)
            log_core(ofs, "[ERROR] ", p1, p2);
        else
            failedToOpenLogFile();
    }
    template <typename T1, typename T2, typename T3>
    static void error(const T1& p1, const T2& p2, const T3& p3)
    {
        std::ofstream ofs(error_file_path_.c_str(), std::ios::app);
        if (ofs)
            log_core(ofs, "[ERROR] ", p1, p2, p3);
        else
            failedToOpenLogFile();
    }
    template <typename T1, typename T2, typename T3, typename T4>
    static void error(const T1& p1, const T2& p2, const T3& p3, const T4& p4)
    {
        std::ofstream ofs(error_file_path_.c_str(), std::ios::app);
        if (ofs)
            log_core(ofs, "[ERROR] ", p1, p2, p3, p4);
        else
            failedToOpenLogFile();
    }
    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    static void error(
        const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5)
    {
        std::ofstream ofs(error_file_path_.c_str(), std::ios::app);
        if (ofs)
            log_core(ofs, "[ERROR] ", p1, p2, p3, p4, p5);
        else
            failedToOpenLogFile();
    }
    // infoログ出力（可変引数対応）
    template <typename T1>
    static void info(const T1& p1)
    {
        log_core(std::cerr, "[INFO] ", p1);
    }
    template <typename T1, typename T2>
    static void info(const T1& p1, const T2& p2)
    {
        log_core(std::cerr, "[INFO] ", p1, p2);
    }
    template <typename T1, typename T2, typename T3>
    static void info(const T1& p1, const T2& p2, const T3& p3)
    {
        log_core(std::cerr, "[INFO] ", p1, p2, p3);
    }
    template <typename T1, typename T2, typename T3, typename T4>
    static void info(const T1& p1, const T2& p2, const T3& p3, const T4& p4)
    {
        log_core(std::cerr, "[INFO] ", p1, p2, p3, p4);
    }
    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    static void info(
        const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5)
    {
        log_core(std::cerr, "[INFO] ", p1, p2, p3, p4, p5);
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
    // debugログ出力（可変引数対応）
    template <typename T1>
    static void debug(const T1& p1)
    {
#ifdef DEBUG
        log_core(std::cerr, "[DEBUG] ", p1);
#endif
    }
    template <typename T1, typename T2>
    static void debug(const T1& p1, const T2& p2)
    {
#ifdef DEBUG
        log_core(std::cerr, "[DEBUG] ", p1, p2);
#endif
    }
    template <typename T1, typename T2, typename T3>
    static void debug(const T1& p1, const T2& p2, const T3& p3)
    {
#ifdef DEBUG
        log_core(std::cerr, "[DEBUG] ", p1, p2, p3);
#endif
    }
    template <typename T1, typename T2, typename T3, typename T4>
    static void debug(const T1& p1, const T2& p2, const T3& p3, const T4& p4)
    {
#ifdef DEBUG
        log_core(std::cerr, "[DEBUG] ", p1, p2, p3, p4);
#endif
    }
    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    static void debug(
        const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5)
    {
#ifdef DEBUG
        log_core(std::cerr, "[DEBUG] ", p1, p2, p3, p4, p5);
#endif
    }

   private:
    Log();
    Log(const Log& other);
    Log& operator=(const Log& other);
    ~Log();

    static std::string error_file_path_;

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
        os << label << p1 << " " << p2 << " " << p3 << std::endl;
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

    static void failedToOpenLogFile()
    {
        std::cerr << "[ERROR] Failed to open log file." << std::endl;
    }
};

std::string Log::error_file_path_ = "./error.txt";

}  // namespace utils

#endif