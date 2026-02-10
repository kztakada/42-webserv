#ifndef WEBSERV_FD_EVENT_HPP_
#define WEBSERV_FD_EVENT_HPP_

namespace server
{
class FdSession;

// fdで発生するイベントの種類
enum FdEventType
{
    kReadEvent,    // FDが読み込み可能（OSから通知）
    kWriteEvent,   // FDが書き込み可能（OSから通知）
    kErrorEvent,   // FDでエラー発生（OSから通知）
    kTimeoutEvent  // セッションがタイムアウト（アプリケーション管理）
};

// ファイルディスクリプタのイベント情報
struct FdEvent
{
    int fd;                  // イベントの発生源fd
    FdEventType type;        // 発生したイベントの種類
    FdSession* session;      // 関連するFdSessionへのポインタ
    bool is_opposite_close;  // ハーフクローズ検出フラグ
};

}  // namespace server

#endif
