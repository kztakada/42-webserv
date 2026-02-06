#include "server/session/fd_session/http_session/actions/send_error_action.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"

namespace server {
using namespace utils::result;

SendErrorAction::SendErrorAction(http::HttpStatus status) : status_(status) {}

Result<void> SendErrorAction::execute(HttpSession& session) {
    RequestProcessor::Output out;
    Result<void> bo = session.buildErrorOutput_(status_, &out);
    if (bo.isError()) return bo;
    session.response_.setHttpVersion(session.request_.getHttpVersion());
    session.installBodySourceAndWriter_(out.body_source);

    session.should_close_connection_ = session.should_close_connection_ || session.peer_closed_ ||
                               out.should_close_connection ||
                               !session.request_.shouldKeepAlive() ||
                               session.dispatcher_.handler().shouldCloseConnection();
    session.changeState(new SendResponseState());
    (void)session.updateSocketWatches_();
    return Result<void>();
}
}
