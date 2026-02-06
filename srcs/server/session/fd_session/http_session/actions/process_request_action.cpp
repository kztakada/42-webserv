#include "server/session/fd_session/http_session/actions/process_request_action.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"

namespace server {
using namespace utils::result;

Result<void> ProcessRequestAction::execute(HttpSession& session) {
    RequestProcessor::Output out;
    Result<void> po = session.buildProcessorOutputOrServerError_(&out);
    if (po.isError()) return po;

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
