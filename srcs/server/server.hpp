#ifndef WEBSERV_SERVER_HPP_
#define WEBSERV_SERVER_HPP_

#include <vector>

#include "http/status.hpp"
#include "server/config/server_config.hpp"
#include "server/reactor/fd_event_reactor.hpp"
#include "server/request_router/request_router.hpp"
#include "server/session/fd_session_controller.hpp"

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
    void restart();
    bool isRunning() const { return should_stop_ == false; }

    // 設定の動的更新
    void reloadConfig(const ServerConfig& config);

   private:
    // for init
    explicit Server(const ServerConfig& config);  // コンストラクタはprivate
    Result<void> initialize();  // 初期化もprivate（Factoryから呼ばれる）

    // コンポーネント
    FdEventReactor* reactor_;
    FdSessionController* session_controller_;
    RequestRouter* router_;

    // 設定オブジェクト
    const ServerConfig& config_;

    // シグナル処理
    static Server* running_instance_;  // 現在実行中のインスタンス
    volatile sig_atomic_t should_stop_;
    static void signalHandler(int signum)
    {
        if (running_instance_)
            running_instance_->should_stop_ = true;
    }

    // コピー禁止
    Server(const Server&);
    Server& operator=(const Server&);

    bool is_running_;

    // for init
    Result<void> registerListenSockets(
        FdEventReactor& fd_event_reactor_, const ServerConfig& config);

    // 内部ヘルパー
    void initializeComponents();
    void cleanupComponents();
};
}  // namespace server

#endif