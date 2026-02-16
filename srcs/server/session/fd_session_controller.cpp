#include "server/session/fd_session_controller.hpp"

#include "server/reactor/fd_event_reactor_factory.hpp"

namespace server
{

FdSessionController::FdSessionController()
    : reactor_(FdEventReactorFactory::create()),
      owns_reactor_(true),
      active_sessions_(),
      deleting_sessions_(),
      deferred_delete_(),
      fd_watch_state_(),
      session_fds_(),
      is_shutting_down_(false)
{
}

FdSessionController::FdSessionController(
    FdEventReactor* reactor, bool owns_reactor)
    : reactor_(reactor),
      owns_reactor_(owns_reactor),
      active_sessions_(),
      deleting_sessions_(),
      deferred_delete_(),
      fd_watch_state_(),
      session_fds_(),
      is_shutting_down_(false)
{
    if (reactor_ == NULL)
    {
        reactor_ = FdEventReactorFactory::create();
        owns_reactor_ = true;
    }
}

FdSessionController::~FdSessionController()
{
    clearAllSessions();
    if (reactor_ != NULL && owns_reactor_)
    {
        delete reactor_;
        reactor_ = NULL;
    }
}

int FdSessionController::getNextTimeoutMs() const
{
    // time(NULL) ベース（秒精度）で十分。handleTimeouts()も同じ精度。
    // 最短の残り秒を探し、そのmsを返す。
    // timeout_seconds_ が 0 以下のものは「timeout無し」とみなす。
    time_t now = time(NULL);

    bool found = false;
    long min_remaining_sec = 0;

    for (std::set<FdSession*>::const_iterator it = active_sessions_.begin();
        it != active_sessions_.end(); ++it)
    {
        FdSession* s = *it;
        if (s == NULL)
            continue;

        const int timeout_sec = s->getTimeoutSeconds();
        if (timeout_sec <= 0)
            continue;

        const long elapsed = static_cast<long>(now - s->getLastActiveTime());
        long remaining = static_cast<long>(timeout_sec) - elapsed;
        if (remaining <= 0)
            return 0;

        if (!found || remaining < min_remaining_sec)
        {
            found = true;
            min_remaining_sec = remaining;
        }
    }

    if (!found)
    {
        // シグナル受信が waitEvents 呼び出し直前（should_stop_ チェック後）に
        // 発生した場合、waitEvents で無限にブロックしてしまうのを防ぐため、
        // タイムアウトを最大1秒に設定する。
        return 1000;
    }

    const long ms = min_remaining_sec * 1000L;
    if (ms < 0 || ms > 1000)
    {
        // 同上の理由により、最大1秒でwaitを解除する。
        return 1000;
    }
    return static_cast<int>(ms);
}

Result<void> FdSessionController::delegateSession(FdSession* session)
{
    if (session == NULL)
        return Result<void>(ERROR, "null session");

    std::vector<FdSession::FdWatchSpec> specs;
    session->getInitialWatchSpecs(&specs);

    // 途中で失敗した場合に rollback できるよう、登録済みfdを記録する。
    std::vector<int> registered_fds;

    active_sessions_.insert(session);
    for (size_t i = 0; i < specs.size(); ++i)
    {
        Result<void> r = setWatch(
            specs[i].fd, session, specs[i].watch_read, specs[i].watch_write);
        if (r.isError())
        {
            for (size_t j = 0; j < registered_fds.size(); ++j)
                unregisterFd(registered_fds[j]);
            active_sessions_.erase(session);
            return r;
        }

        if (specs[i].watch_read || specs[i].watch_write)
            registered_fds.push_back(specs[i].fd);
    }
    return Result<void>();
}

void FdSessionController::requestDelete(FdSession* session)
{
    if (session == NULL)
        return;
    if (deleting_sessions_.find(session) != deleting_sessions_.end())
        return;

    deleting_sessions_.insert(session);
    active_sessions_.erase(session);

    // 関連fdのwatchを解除
    detachAllFdsFromSession_(session);
    // session_fds_ が壊れていても reactor watch
    // が残らないように保険で掃除する。
    detachAllFdsFromSessionFallback_(session);

    // dispatch バッチ末尾で delete
    deferred_delete_.push_back(session);
}

void FdSessionController::detachAllFdsFromSessionFallback_(FdSession* session)
{
    if (session == NULL)
        return;

    std::vector<int> fds;
    for (std::map<int, FdWatchState>::iterator it = fd_watch_state_.begin();
        it != fd_watch_state_.end(); ++it)
    {
        if (it->second.session == session)
            fds.push_back(it->first);
    }

    for (size_t i = 0; i < fds.size(); ++i)
    {
        const int fd = fds[i];
        if (reactor_ != NULL)
        {
            (void)reactor_->deleteWatch(fd);
        }
        detachFdFromSession_(fd);
    }
}

Result<void> FdSessionController::setWatch(
    int fd, FdSession* session, bool watch_read, bool watch_write)
{
    return addOrRemoveWatch_(fd, session, watch_read, watch_write);
}

Result<void> FdSessionController::updateWatch(
    int fd, bool watch_read, bool watch_write)
{
    std::map<int, FdWatchState>::iterator it = fd_watch_state_.find(fd);
    if (it == fd_watch_state_.end())
        return Result<void>(ERROR, "fd not registered");
    return addOrRemoveWatch_(fd, it->second.session, watch_read, watch_write);
}

Result<void> FdSessionController::rebindWatch(
    int fd, FdSession* new_session, bool watch_read, bool watch_write)
{
    unregisterFd(fd);
    return setWatch(fd, new_session, watch_read, watch_write);
}

void FdSessionController::unregisterFd(int fd)
{
    if (reactor_ != NULL)
    {
        (void)reactor_->deleteWatch(fd);
    }
    detachFdFromSession_(fd);
}

Result<void> FdSessionController::addOrRemoveWatch_(
    int fd, FdSession* session, bool want_read, bool want_write)
{
    if (reactor_ == NULL)
        return Result<void>(ERROR, "reactor is null");
    if (fd < 0)
        return Result<void>(ERROR, "invalid fd");
    if (session == NULL)
        return Result<void>(ERROR, "null session");

    std::map<int, FdWatchState>::iterator it = fd_watch_state_.find(fd);
    if (it != fd_watch_state_.end())
    {
        if (it->second.session != session)
            return Result<void>(ERROR, "fd already bound to another session");
    }

    const bool have_read =
        (it != fd_watch_state_.end()) ? it->second.watch_read : false;
    const bool have_write =
        (it != fd_watch_state_.end()) ? it->second.watch_write : false;

    // add
    if (want_read && !have_read)
    {
        FdEvent ev;
        ev.fd = fd;
        ev.type = kReadEvent;
        ev.session = session;
        ev.is_opposite_close = false;
        Result<void> r = reactor_->addWatch(ev);
        if (r.isError())
            return r;
    }
    if (want_write && !have_write)
    {
        FdEvent ev;
        ev.fd = fd;
        ev.type = kWriteEvent;
        ev.session = session;
        ev.is_opposite_close = false;
        Result<void> r = reactor_->addWatch(ev);
        if (r.isError())
            return r;
    }

    // remove
    if (!want_read && have_read)
    {
        FdEvent ev;
        ev.fd = fd;
        ev.type = kReadEvent;
        ev.session = session;
        ev.is_opposite_close = false;
        (void)reactor_->removeWatch(ev);
    }
    if (!want_write && have_write)
    {
        FdEvent ev;
        ev.fd = fd;
        ev.type = kWriteEvent;
        ev.session = session;
        ev.is_opposite_close = false;
        (void)reactor_->removeWatch(ev);
    }

    // 状態更新
    if (want_read || want_write)
    {
        fd_watch_state_[fd] = FdWatchState(session, want_read, want_write);
        session_fds_[session].insert(fd);
    }
    else
    {
        unregisterFd(fd);
    }

    return Result<void>();
}

void FdSessionController::detachFdFromSession_(int fd)
{
    std::map<int, FdWatchState>::iterator it = fd_watch_state_.find(fd);
    if (it == fd_watch_state_.end())
        return;

    FdSession* s = it->second.session;
    fd_watch_state_.erase(it);

    std::map<FdSession*, std::set<int> >::iterator sit = session_fds_.find(s);
    if (sit == session_fds_.end())
        return;
    sit->second.erase(fd);
    if (sit->second.empty())
        session_fds_.erase(sit);
}

void FdSessionController::detachAllFdsFromSession_(FdSession* session)
{
    std::map<FdSession*, std::set<int> >::iterator it =
        session_fds_.find(session);
    if (it == session_fds_.end())
        return;

    std::set<int> fds = it->second;
    for (std::set<int>::iterator fit = fds.begin(); fit != fds.end(); ++fit)
    {
        const int fd = *fit;
        if (reactor_ != NULL)
        {
            (void)reactor_->deleteWatch(fd);
        }
        detachFdFromSession_(fd);
    }
}

void FdSessionController::dispatchEvents(
    const std::vector<FdEvent>& occurred_events)
{
    for (size_t i = 0; i < occurred_events.size(); ++i)
    {
        const FdEvent& event = occurred_events[i];
        FdSession* session = event.session;
        if (session == NULL)
        {
            continue;
        }
        // reactor 側に「削除済みSessionのイベント」が残ることがあるため、
        // controller が管理していない pointer は絶対に触らない。
        if (active_sessions_.find(session) == active_sessions_.end() &&
            deleting_sessions_.find(session) == deleting_sessions_.end())
        {
            continue;
        }
        if (deleting_sessions_.find(session) != deleting_sessions_.end())
            continue;

        session->handleEvent(event);
        if (deleting_sessions_.find(session) != deleting_sessions_.end())
            continue;

        // controller 主導でも回収する（session側が requestDelete
        // を呼ばなくても安全）
        if (session->isComplete())
        {
            requestDelete(session);
        }
    }

    // dispatch 中に delete すると UAF になるため、末尾でまとめて破棄する。
    for (size_t i = 0; i < deferred_delete_.size(); ++i)
    {
        delete deferred_delete_[i];
    }
    deferred_delete_.clear();
    deleting_sessions_.clear();
}

void FdSessionController::handleTimeouts()
{
    std::vector<FdSession*> timed_out;
    for (std::set<FdSession*>::iterator it = active_sessions_.begin();
        it != active_sessions_.end(); ++it)
    {
        FdSession* s = *it;
        if (s != NULL && s->isTimedOut())
            timed_out.push_back(s);
    }

    for (size_t i = 0; i < timed_out.size(); ++i)
    {
        FdEvent ev;
        ev.fd = -1;
        ev.type = kTimeoutEvent;
        ev.session = timed_out[i];
        ev.is_opposite_close = false;

        if (deleting_sessions_.find(timed_out[i]) != deleting_sessions_.end())
            continue;

        timed_out[i]->handleEvent(ev);
        requestDelete(timed_out[i]);
    }
}

void FdSessionController::clearAllSessions()
{
    is_shutting_down_ = true;

    // watch解除
    if (reactor_ != NULL)
    {
        reactor_->clearAllEvents();
    }
    fd_watch_state_.clear();
    session_fds_.clear();

    for (std::set<FdSession*>::iterator it = active_sessions_.begin();
        it != active_sessions_.end(); ++it)
    {
        delete *it;
    }
    active_sessions_.clear();

    for (size_t i = 0; i < deferred_delete_.size(); ++i)
        delete deferred_delete_[i];
    deferred_delete_.clear();
    deleting_sessions_.clear();

    is_shutting_down_ = false;
}

}  // namespace server
