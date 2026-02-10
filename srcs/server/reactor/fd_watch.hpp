#ifndef WEBSERV_FD_WATCH_HPP_
#define WEBSERV_FD_WATCH_HPP_

#include <stdint.h>

#include <vector>

#include "server/reactor/fd_event.hpp"

namespace server
{
class FdSession;

// reactor内部のwatch用イベントビット（FdEventTypeとは独立）
enum FdEventMask
{
    kReadEventMask = 0x0001,
    kWriteEventMask = 0x0002
};

// 監視するファイルディスクリプタの情報
struct FdWatch
{
    int fd;              // 監視対象のfd
    uint32_t events;     // 監視するイベントの種類（FdEventMaskのビット和）
    FdSession* session;  // 関連するFdSessionへのポインタ

    FdWatch(int fd, uint32_t events, FdSession* session)
        : fd(fd), events(events), session(session)
    {
    }
    ~FdWatch() {}

    std::vector<FdEvent> makeFdEvents(
        uint32_t triggered_events, bool is_opposite_close);
};

}  // namespace server

#endif
