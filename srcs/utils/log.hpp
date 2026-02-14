#ifndef UTILS_LOG_HPP_
#define UTILS_LOG_HPP_

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "network/ip_address.hpp"
#include "network/port_type.hpp"
#include "utils/path.hpp"
#include "utils/timestamp.hpp"

namespace utils
{
class Log;
class ProcessingLog;

class ProcessingLog
{
   public:
    ProcessingLog();

    void run();
    void stop();

    // Server のイベントループから定期的に呼び出す。
    void tick(bool force_flush);
    void tick() { tick(false); }

    // --- 更新API ---
    void onConnectionOpened() { ++active_connections_; }
    void onConnectionClosed();

    void onCgiStarted() { ++cgi_count_; }
    void onCgiFinished();

    void recordLoopTimeSeconds(long seconds);

    void recordRequestTimeSeconds(long seconds);

    void incrementBlockIo() { ++block_io_count_; }

    void clearFile();

   private:
    bool is_running_;
    long last_flush_epoch_seconds_;

    // snapshot系（最新値を保持して出力する）
    long active_connections_;
    long cgi_count_;

    // period系（1秒毎に集計してクリアする）
    long loop_time_max_seconds_;
    long req_time_max_seconds_;
    long block_io_count_;

    std::vector<std::string> cached_lines_;

    static const char* getFilePath_() { return "./logs/processing.txt"; }

    void resetPeriodMetrics_()
    {
        loop_time_max_seconds_ = 0;
        req_time_max_seconds_ = 0;
        block_io_count_ = 0;
    }

    void cacheCurrentLine_();
    void flushCache_();
};

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
        WARNING,
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
    // warningログ出力
    template <typename T1>
    static void warning(const T1& p1)
    {
        std::ofstream ofs(getFilePath(WARNING), std::ios::app);
        if (ofs)
            log_core(ofs, "[WARNING] ", Timestamp::now(), p1);
        else
            failedToOpenLogFile(WARNING);
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
    static void clearFiles()
    {
        std::ofstream ofs(getFilePath(INFO), std::ios::trunc);
        ofs.close();
        ofs.open(getFilePath(ERROR), std::ios::trunc);
        ofs.close();
        ofs.open(getFilePath(WARNING), std::ios::trunc);
        ofs.close();
        ofs.open(getFilePath(F_DEBUG), std::ios::trunc);
        ofs.close();
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
        if (type == WARNING)
            return "./logs/warning.txt";
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

    // 引数4つの実装
    template <typename T1, typename T2, typename T3, typename T4>
    static void log_core(std::ostream& os, const char* label, const T1& p1,
        const T2& p2, const T3& p3, const T4& p4)
    {
        os << label << p1 << " " << p2 << " " << p3 << " " << p4 << std::endl;
    }

    static void failedToOpenLogFile(FileType type)
    {
        std::cerr << "[ERROR] Failed to open log file." << " at "
                  << getFilePath(type) << std::endl;
    }
};

}  // namespace utils

#endif
