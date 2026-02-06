#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{
using namespace utils::result;

Result<void> CloseWaitState::handleEvent(HttpSession& context, const FdEvent& event) {
    (void)event;
    context.is_complete_ = true;
    context.controller_.requestDelete(&context);
    return Result<void>();
}

}  // namespace server
