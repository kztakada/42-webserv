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

    session.context_.should_close_connection = session.context_.should_close_connection || session.context_.peer_closed ||
                               out.should_close_connection ||
                               !session.context_.request.shouldKeepAlive() ||
                               session.dispatcher_.handler().shouldCloseConnection();
    session.changeState(new SendResponseState());
    (void)session.updateSocketWatches_();
    return Result<void>();
}
}
