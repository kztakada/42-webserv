#include "log.hpp"

namespace utils
{

// public --------------------------
ProcessingLog::ProcessingLog()
    : is_running_(false),
      last_flush_epoch_seconds_(0),
      active_connections_(0),
      cgi_count_(0),
      loop_time_max_seconds_(0),
      req_time_max_seconds_(0),
      block_io_count_(0),
      cached_lines_()
{
}

void ProcessingLog::run()
{
#ifdef DEBUG
    is_running_ = true;
    last_flush_epoch_seconds_ = Timestamp::nowEpochSeconds();
#else
    is_running_ = false;
#endif
}

void ProcessingLog::stop()
{
    if (!is_running_)
        return;
    tick(true);
    is_running_ = false;
}

void ProcessingLog::tick(bool force_flush)
{
    if (!is_running_)
        return;

    const long now = Timestamp::nowEpochSeconds();
    if (!force_flush && (now - last_flush_epoch_seconds_) < 1)
        return;

    cacheCurrentLine_();
    flushCache_();
    resetPeriodMetrics_();
    last_flush_epoch_seconds_ = now;
}

void ProcessingLog::onConnectionClosed()
{
    if (active_connections_ > 0)
        --active_connections_;
}

void ProcessingLog::onCgiFinished()
{
    if (cgi_count_ > 0)
        --cgi_count_;
}

void ProcessingLog::recordLoopTimeSeconds(long seconds)
{
    if (seconds > loop_time_max_seconds_)
        loop_time_max_seconds_ = seconds;
}

void ProcessingLog::recordRequestTimeSeconds(long seconds)
{
    if (seconds > req_time_max_seconds_)
        req_time_max_seconds_ = seconds;
}

void ProcessingLog::clearFile()
{
    std::ofstream ofs(getFilePath_(), std::ios::trunc);
    ofs.close();
}

// private --------------------------

void ProcessingLog::cacheCurrentLine_()
{
    std::ostringstream oss;
    oss << Timestamp::now() << ", " << active_connections_ << ", "
        << loop_time_max_seconds_ << ", " << req_time_max_seconds_ << ", "
        << cgi_count_ << ", " << block_io_count_;
    cached_lines_.push_back(oss.str());
}

void ProcessingLog::flushCache_()
{
    if (cached_lines_.empty())
        return;

    std::ofstream ofs(getFilePath_(), std::ios::app);
    if (!ofs)
    {
        std::cerr << "[ERROR] Failed to open log file." << " at "
                  << getFilePath_() << std::endl;
        cached_lines_.clear();
        return;
    }

    for (size_t i = 0; i < cached_lines_.size(); ++i)
    {
        ofs << cached_lines_[i] << std::endl;
    }
    cached_lines_.clear();
}

}  // namespace utils
