#ifndef WEBSERV_SERVER_HPP_
#define WEBSERV_SERVER_HPP_

#include <csignal>
#include <vector>

#include "http/status.hpp"
#include "server/config/server_config.hpp"
#include "server/http_processing_module/http_processing_module.hpp"
#include "server/reactor/fd_event_reactor.hpp"
#include "server/session/fd_session_controller.hpp"
#include "utils/log.hpp"
#include "utils/result.hpp"

namespace server
{

using namespace http;
using namespace utils::result;

class Server
{
   public:
    // Factory method: 初期化済みのServerを作成
    static Result<Server*> create(const ServerConfig& config);

    ~Server();

    // サーバーのライフサイクル管理
    void start();
    void stop();
    bool isRunning() const { return should_stop_ == false; }

   private:
    // accept()されるまで待ち状態にしておける接続要求のキュー（保留）の上限
    static const int kListenBacklog = 128;

    static Server* running_instance_;  // 現在実行中のインスタンス
    bool is_running_;                  // サーバーの状態

    // コンポーネント
    FdEventReactor* reactor_;
    FdSessionController* session_controller_;
    HttpProcessingModule* http_processing_module_;

    // 処理負荷計測ログ（1インスタンス）
    utils::ProcessingLog processing_log_;

    // 設定オブジェクト
    ServerConfig config_;

    // シグナル処理
    static void signalHandler(int signum);
    volatile sig_atomic_t should_stop_;

    // for init
    explicit Server(const ServerConfig& config);  // コンストラクタはprivate
    Result<void> initialize();                    // 動的初期化(Factory専用)

    // コピー禁止
    Server(const Server&);
    Server& operator=(const Server&);
};
}  // namespace server

#endif
