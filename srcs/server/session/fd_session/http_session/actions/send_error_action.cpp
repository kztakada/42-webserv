#include "server/session/fd_session/http_session/actions/send_error_action.hpp"

#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"

namespace server
{
using namespace utils::result;

SendErrorAction::SendErrorAction(http::HttpStatus status) : status_(status) {}

Result<void> SendErrorAction::execute(HttpSession& session)
{
    RequestProcessor::Output out;
    Result<void> bo = session.buildErrorOutput_(status_, &out);
    if (bo.isError())
        return bo;
    session.context_.response.setHttpVersion(
        session.context_.request.getHttpVersion());
    session.installBodySourceAndWriter_(out.body_source);

    session.context_.should_close_connection =
        session.context_.should_close_connection ||
        session.context_.peer_closed || out.should_close_connection ||
        !session.context_.request.shouldKeepAlive() ||
        session.getContext().request_handler.shouldCloseConnection();
    session.changeState(new SendResponseState());
    (void)session.updateSocketWatches_();
    return Result<void>();
}
}  // namespace server
