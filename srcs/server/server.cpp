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
    : reactor_(FdEventReactorFactory::create()),
      session_controller_(NULL),
      router_(NULL),
      config_(config),
      should_stop_(0),
      is_running_(false)
{
    session_controller_ = new FdSessionController(reactor_, false);
    router_ = new RequestRouter(config_);
}

Server::~Server()
{
    stop();

    if (session_controller_ != NULL)
    {
        delete session_controller_;
        session_controller_ = NULL;
    }
    if (router_ != NULL)
    {
        delete router_;
        router_ = NULL;
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

    while (!should_stop_)
    {
        // 1. 次のタイムアウト時間を計算
        int timeout_ms = session_controller_->getNextTimeoutMs();
        // 2. イベント待機
        Result<const std::vector<FdEvent>&> events_result =
            reactor_->waitEvents(timeout_ms);
        if (events_result.isError())
        {
            if (should_stop_)
                break;
            Log::error("Event wait failed: ", events_result.getErrorMessage());
            continue;
        }
        // イベント取得成功
        const std::vector<FdEvent>& occurred_events = events_result.unwrap();

        // 3. イベント処理
        session_controller_->dispatchEvents(occurred_events);

        // 4. タイムアウト処理
        session_controller_->handleTimeouts();
    }

    session_controller_->clearAllSessions();
    reactor_->clearAllEvents();
    Log::info("Server stopped.");
    is_running_ = false;
}

void Server::stop()
{
    if (running_instance_ == this)
        running_instance_ = NULL;
    should_stop_ = true;
}

//------------------------------------------------------------
// private

Result<void> Server::initialize()
{
    if (!config_.isValid())
        return Result<void>(ERROR, "server config is invalid");

    if (reactor_ == NULL || session_controller_ == NULL || router_ == NULL)
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

        ListenerSession* listener = new ListenerSession(
            listen_fd.unwrap(), *session_controller_, *router_);

        Result<void> d = session_controller_->delegateSession(listener);
        if (d.isError())
        {
            delete listener;
            return Result<void>(ERROR, d.getErrorMessage());
        }
    }

    return Result<void>();
}

}  // namespace server