#ifndef WEBSERV_FD_EVENT_HPP_
#define WEBSERV_FD_EVENT_HPP_

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
    int fd;  // ファイルディスクリプタ
    enum Role
    {
        LISTENER,
        CONNECTION,
        CGI_IN,
        CGI_OUT,
        CGI_ERR
    } role;              // FDの役割
    FdSession* session;  // 関連するFdSessionへのポインタ
};

}  // namespace server

#endif