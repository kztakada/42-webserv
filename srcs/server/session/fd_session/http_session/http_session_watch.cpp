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

    context_.socket_watch_read = want_read;
    context_.socket_watch_write = want_write;

    Result<void> u = controller_.updateWatch(fd, want_read, want_write);
    if (u.isError())
        return Result<void>(ERROR, u.getErrorMessage());
    return Result<void>();
}

}  // namespace server