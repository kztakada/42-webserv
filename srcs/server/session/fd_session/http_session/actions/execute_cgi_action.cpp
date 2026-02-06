#include "server/session/fd_session/http_session/actions/execute_cgi_action.hpp"

#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/actions/send_error_action.hpp"

namespace server
{
using namespace utils::result;

Result<void> ExecuteCgiAction::execute(HttpSession& session)
{
    Result<void> sr = session.module().cgi_handler.startCgi(session);
    if (sr.isError())
    {
        SendErrorAction fallback(http::HttpStatus::SERVER_ERROR);
        return fallback.execute(session);
    }
    return Result<void>();
}
}  // namespace server