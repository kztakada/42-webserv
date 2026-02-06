#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session/http_session/actions/send_error_action.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::prepareResponseOrCgi_()
{
    response_.reset();

    Result<IRequestAction*> action_r = dispatcher_.dispatch();
    if (action_r.isError())
    {
        // ディスパッチ失敗（通常はActionを返すが、システムエラーの場合）
        return Result<void>(ERROR, action_r.getErrorMessage());
    }

    IRequestAction* action = action_r.unwrap();
    Result<void> r = action->execute(*this);
    delete action;
    return r;
}

Result<void> HttpSession::consumeRecvBufferWithoutRead_()
{
    if (pending_state_ != NULL)
        return Result<void>();

    for (;;)
    {
        const size_t before = recv_buffer_.size();

        Result<void> c = dispatcher_.consumeFromRecvBuffer(recv_buffer_);
        if (c.isError())
        {
            http::HttpStatus st = request_.getParseErrorStatus();
            if (st == http::HttpStatus::OK)
                st = http::HttpStatus::BAD_REQUEST;

            SendErrorAction action(st);
            return action.execute(*this);
        }

        if (request_.isParseComplete())
            return prepareResponseOrCgi_();

        const size_t after = recv_buffer_.size();

        if (after >= before)
            break;
    }
    return Result<void>();
}

}  // namespace server