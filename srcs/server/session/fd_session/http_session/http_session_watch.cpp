#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::updateSocketWatches_()
{
    const int fd = context_.socket_fd.getFd();
    if (fd < 0)
        return Result<void>();

    bool want_read = false;
    bool want_write = false;

    IHttpSessionState* s = context_.pending_state ? context_.pending_state
                                                  : context_.current_state;

    if (s != NULL)
        s->getWatchFlags(*this, &want_read, &want_write);

    if (want_read == context_.socket_watch_read &&
        want_write == context_.socket_watch_write)
        return Result<void>();

    // want_read と want_write が両方 false の場合、Controller は fd を
    // unregister する。 その後に write/read を再度有効化する際、updateWatch()
    // だけだと "fd not registered" になってしまうため、必要に応じて setWatch()
    // で復帰させる。
    Result<void> u = controller_.updateWatch(fd, want_read, want_write);
    if (u.isError())
    {
        Result<void> s = controller_.setWatch(fd, this, want_read, want_write);
        if (s.isError())
            return Result<void>(ERROR, s.getErrorMessage());
    }

    context_.socket_watch_read = want_read;
    context_.socket_watch_write = want_write;
    return Result<void>();
}

Result<void> HttpSession::setBodyWatchFd_(int fd)
{
    if (fd < 0)
        return Result<void>(ERROR, "invalid body fd");

    // 既存の watch が別 fd なら解除
    if (context_.body_watch_fd >= 0 && context_.body_watch_fd != fd)
    {
        controller_.unregisterFd(context_.body_watch_fd);
        context_.body_watch_fd = -1;
        context_.body_watch_read = false;
    }

    if (context_.body_watch_fd == fd && context_.body_watch_read)
        return Result<void>();

    // 登録/更新
    if (context_.body_watch_fd < 0)
    {
        Result<void> r = controller_.setWatch(fd, this, true, false);
        if (r.isError())
            return r;
    }
    else
    {
        Result<void> r = controller_.updateWatch(fd, true, false);
        if (r.isError())
            return r;
    }

    context_.body_watch_fd = fd;
    context_.body_watch_read = true;
    return Result<void>();
}

void HttpSession::clearBodyWatch_()
{
    if (context_.body_watch_fd >= 0)
    {
        controller_.unregisterFd(context_.body_watch_fd);
        context_.body_watch_fd = -1;
    }
    context_.body_watch_read = false;
}

}  // namespace server