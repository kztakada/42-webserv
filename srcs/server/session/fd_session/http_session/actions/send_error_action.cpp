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

    // 受信途中/パースエラーのときは、次にどこから再開できるか（ストリーム同期）
    // が保証できないため close する。
    // 一方で、すでにリクエストが確定している場合の 5xx/4xx は
    // keep-alive を維持しても問題ない（close が必要な場合は encoder
    // が決める）。
    const bool should_close = session.context_.request.hasParseError() ||
                              !session.context_.request.isParseComplete();
    if (should_close)
    {
        (void)session.context_.response.setHeader("Connection", "close");
    }
    session.installBodySourceAndWriter_(out.body_source);

    session.context_.should_close_connection =
        session.context_.should_close_connection || should_close;
    session.changeState(new SendResponseState());
    (void)session.updateSocketWatches_();
    return Result<void>();
}
}  // namespace server
