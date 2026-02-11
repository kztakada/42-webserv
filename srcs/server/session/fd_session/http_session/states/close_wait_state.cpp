#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{
using namespace utils::result;

Result<void> CloseWaitState::handleEvent(
    HttpSession& context, const FdEvent& event)
{
    (void)event;
    context.cleanupCgiOnClose_();
    context.context_.is_complete = true;
    context.controller_.requestDelete(&context);
    return Result<void>();
}

void CloseWaitState::getWatchFlags(
    const HttpSession& session, bool* want_read, bool* want_write) const
{
    (void)session;
    if (want_read)
    {
        *want_read = false;
    }
    if (want_write)
    {
        *want_write = false;
    }
}

}  // namespace server
