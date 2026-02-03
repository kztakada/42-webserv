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

static Result<uint32_t> parseIpv4ToNetwork_(const std::string& ip_str)
{
    // IPAddress::toString() は数値IPv4のみが前提。
    // inet_pton/inet_aton は禁止関数のため、自前でパースする。
    unsigned int octets[4];
    unsigned int octet_index = 0;
    unsigned int current = 0;
    bool has_digit = false;
    unsigned int digit_count = 0;

    for (size_t i = 0; i < ip_str.size(); ++i)
    {
        const char c = ip_str[i];
        if (c >= '0' && c <= '9')
        {
            has_digit = true;
            ++digit_count;
            if (digit_count > 3)
                return Result<uint32_t>(ERROR, "invalid ip");
            current = current * 10u + static_cast<unsigned int>(c - '0');
            if (current > 255u)
                return Result<uint32_t>(ERROR, "invalid ip");
        }
        else if (c == '.')
        {
            if (!has_digit)
                return Result<uint32_t>(ERROR, "invalid ip");
            if (octet_index >= 4)
                return Result<uint32_t>(ERROR, "invalid ip");
            octets[octet_index] = current;
            ++octet_index;
            current = 0;
            has_digit = false;
            digit_count = 0;
        }
        else
        {
            return Result<uint32_t>(ERROR, "invalid ip");
        }
    }

    if (!has_digit)
        return Result<uint32_t>(ERROR, "invalid ip");
    if (octet_index != 3)
        return Result<uint32_t>(ERROR, "invalid ip");
    octets[3] = current;

    const uint32_t host_order =
        (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | (octets[3]);
    return htonl(host_order);
}

static Result<unsigned int> parsePortNumber_(const std::string& port_str)
{
    if (port_str.empty())
        return Result<unsigned int>(ERROR, "port is empty");
    unsigned long value = 0;
    for (size_t i = 0; i < port_str.size(); ++i)
    {
        const char c = port_str[i];
        if (c < '0' || c > '9')
            return Result<unsigned int>(ERROR, "port must be numeric");
        value = value * 10ul + static_cast<unsigned long>(c - '0');
        if (value > 65535ul)
            return Result<unsigned int>(ERROR, "port is out of range");
    }
    if (value == 0ul)
        return Result<unsigned int>(ERROR, "port is out of range");
    return static_cast<unsigned int>(value);
}

static Result<void> setNonBlocking_(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return Result<void>(ERROR, "fcntl(F_GETFL) failed");
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return Result<void>(ERROR, "fcntl(F_SETFL) failed");
    return Result<void>();
}

static Result<int> createListenFd_(const Listen& listen)
{
    Result<unsigned int> port = parsePortNumber_(listen.port.toString());
    if (port.isError())
        return Result<int>(ERROR, port.getErrorMessage());

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return Result<int>(ERROR, "socket() failed");

    int yes = 1;
    (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port.unwrap()));

    if (listen.host_ip.isWildcard())
    {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else
    {
        Result<uint32_t> saddr = parseIpv4ToNetwork_(listen.host_ip.toString());
        if (saddr.isError())
        {
            ::close(fd);
            return Result<int>(ERROR, "listen ip is invalid");
        }
        addr.sin_addr.s_addr = saddr.unwrap();
    }

    if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(fd);
        return Result<int>(ERROR, "bind() failed");
    }
    if (::listen(fd, 128) < 0)
    {
        ::close(fd);
        return Result<int>(ERROR, "listen() failed");
    }

    Result<void> nb = setNonBlocking_(fd);
    if (nb.isError())
    {
        ::close(fd);
        return Result<int>(ERROR, nb.getErrorMessage());
    }

    return fd;
}

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
    should_stop_ = 0;
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
    should_stop_ = 1;
}

void Server::restart()
{
    // 最小実装: stop はフラグを立てるだけ。
    // start() はブロッキングなので、呼び出し側が適切に制御する想定。
    stop();
}

void Server::reloadConfig(const ServerConfig& config)
{
    // 最小実装: 動的更新は別途。
    // 将来は listener/router の差し替えや graceful restart を行う。
    (void)config;
}

//------------------------------------------------------------
// private

Result<void> Server::initialize()
{
    if (!config_.isValid())
    {
        return Result<void>(ERROR, "server config is invalid");
    }
    if (reactor_ == NULL || session_controller_ == NULL || router_ == NULL)
    {
        return Result<void>(ERROR, "server components are not initialized");
    }

    std::vector<Listen> listens = config_.getListens();
    if (listens.empty())
    {
        return Result<void>(ERROR, "no listen endpoints");
    }

    for (size_t i = 0; i < listens.size(); ++i)
    {
        Result<int> fd = createListenFd_(listens[i]);
        if (fd.isError())
        {
            return Result<void>(ERROR, fd.getErrorMessage());
        }

        // bind() 実際のアドレスを SocketAddress に保持する
        struct sockaddr_in bound;
        std::memset(&bound, 0, sizeof(bound));
        socklen_t len = sizeof(bound);
        if (::getsockname(fd.unwrap(), (struct sockaddr*)&bound, &len) != 0)
        {
            ::close(fd.unwrap());
            return Result<void>(ERROR, "getsockname() failed");
        }

        ListenerSession* listener = new ListenerSession(
            fd.unwrap(), SocketAddress(bound), *session_controller_, *router_);
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