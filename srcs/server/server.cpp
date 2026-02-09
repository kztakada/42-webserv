#include "server/server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstring>
#include <sstream>

#include "server/reactor/fd_event_reactor_factory.hpp"
#include "server/session/fd_session/listener_session.hpp"
#include "utils/log.hpp"

namespace server
{
using namespace utils;
using namespace utils::result;

Server* Server::running_instance_ = NULL;

Result<Server*> Server::create(const ServerConfig& config)
{
    Server* s = new Server(config);
    Result<void> init = s->initialize();
    if (init.isError())
    {
        const std::string msg = init.getErrorMessage();
        delete s;
        return Result<Server*>(ERROR, msg);
    }
    return s;
}

Server::Server(const ServerConfig& config)
    : is_running_(false),
      reactor_(FdEventReactorFactory::create()),
      session_controller_(NULL),
      http_processing_module_(NULL),
      config_(config),
      should_stop_(0)
{
    session_controller_ = new FdSessionController(reactor_, false);
    http_processing_module_ =
        new HttpProcessingModule(config_, *session_controller_);
}

Server::~Server()
{
    stop();

    if (session_controller_ != NULL)
    {
        delete session_controller_;
        session_controller_ = NULL;
    }
    if (http_processing_module_ != NULL)
    {
        delete http_processing_module_;
        http_processing_module_ = NULL;
    }
    if (reactor_ != NULL)
    {
        delete reactor_;
        reactor_ = NULL;
    }

    if (running_instance_ == this)
        running_instance_ = NULL;
}

void Server::start()
{
    should_stop_ = false;
    is_running_ = true;

    running_instance_ = this;

    signal(SIGINT, signalHandler);   // Ctrl+C
    signal(SIGTERM, signalHandler);  // kill
    signal(SIGPIPE, SIG_IGN);        // SIGPIPE無視（write時のエラーで処理）

    utils::Log::clearFiles();
    processing_log_.clearFile();

    processing_log_.run();  // ログ計測
    Log::display("Server started.");
    while (!should_stop_)
    {
        // 1. 次のタイムアウト時間を計算
        // getNextTimeoutMs() 内で最大1秒(1000ms)に制限されており、
        // 定期的に should_stop_ チェックが走るようになっている。
        int timeout_ms = session_controller_->getNextTimeoutMs();

        // 2. イベント待機
        Result<const std::vector<FdEvent>&> events_result =
            reactor_->waitEvents(timeout_ms);
        if (events_result.isError())
        {
            if (should_stop_)
                break;
            Log::error("Server",
                "Event wait failed:", events_result.getErrorMessage());
            processing_log_.tick();  // ログ計測
            continue;
        }
        // イベント取得成功
        const std::vector<FdEvent>& occurred_events = events_result.unwrap();

        // ログ計測
        const long loop_start_seconds = utils::Timestamp::nowEpochSeconds();

        // 3. イベント処理
        session_controller_->dispatchEvents(occurred_events);

        // 4. タイムアウト処理
        session_controller_->handleTimeouts();

        // ログ計測
        const long loop_end_seconds = utils::Timestamp::nowEpochSeconds();
        const long loop_time_seconds = loop_end_seconds - loop_start_seconds;
        if (loop_time_seconds > 0)
            processing_log_.recordLoopTimeSeconds(loop_time_seconds);
        else
            processing_log_.recordLoopTimeSeconds(0);
        processing_log_.tick();
    }

    session_controller_->clearAllSessions();
    reactor_->clearAllEvents();
    Log::display("Server stopped.");
    is_running_ = false;

    processing_log_.stop();  // ログ計測
}

void Server::stop()
{
    if (running_instance_ == this)
        running_instance_ = NULL;
    should_stop_ = true;
}

//------------------------------------------------------------
// private

void Server::signalHandler(int signum)
{
    (void)signum;
    if (running_instance_)
        running_instance_->should_stop_ = true;
}

Result<void> Server::initialize()
{
    if (!config_.isValid())
        return Result<void>(ERROR, "server config is invalid");

    if (reactor_ == NULL || session_controller_ == NULL ||
        http_processing_module_ == NULL)
        return Result<void>(ERROR, "server components are not initialized");

    std::vector<Listen> listens = config_.getListens();
    if (listens.empty())
        return Result<void>(ERROR, "no listen endpoints");

    for (size_t i = 0; i < listens.size(); ++i)
    {
        Result<TcpListenSocketFd*> listen_fd = TcpListenSocketFd::listenOn(
            listens[i].host_ip, listens[i].port, kListenBacklog);
        if (listen_fd.isError())
            return Result<void>(ERROR, listen_fd.getErrorMessage());

        ListenerSession* listener = new ListenerSession(listen_fd.unwrap(),
            *session_controller_, *http_processing_module_, &processing_log_);

        Result<void> d = session_controller_->delegateSession(listener);
        if (d.isError())
        {
            delete listener;
            return Result<void>(ERROR, d.getErrorMessage());
        }
        Log::displayListen(listens[i].host_ip, listens[i].port);
    }

    return Result<void>();
}

}  // namespace server