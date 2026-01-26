#ifndef WEBSERV_FD_EVENT_HPP_
#define WEBSERV_FD_EVENT_HPP_

#include <cstdint>
#include <vector>

#include "server/session/fd_session.hpp"

namespace server
{
// fdで発生するイベントの種類
enum FdEventType
{
    kReadEvent = 0x0001,    // FDが読み込み可能（OSから通知）
    kWriteEvent = 0x0002,   // FDが書き込み可能（OSから通知）
    kErrorEvent = 0x0004,   // FDでエラー発生（OSから通知）
    kTimeoutEvent = 0x0008  // セッションがタイムアウト（アプリケーション管理）
};

// ファイルディスクリプタのイベント情報
struct FdEvent
{
    int fd;                  // イベントの発生源fd
    FdEventType type;        // 発生したイベントの種類
    FdSession* session;      // 関連するFdSessionへのポインタ
    bool is_opposite_close;  // ハーフクローズ検出フラグ
};

// 監視するファイルディスクリプタの情報
struct FdWatch
{
    int fd;              // 監視対象のfd
    uint32_t events;     // 監視するイベントの種類（FdEventTypeのビット和）
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