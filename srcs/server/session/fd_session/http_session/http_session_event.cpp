#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session_controller.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::handleEvent(const FdEvent& event)
{
    updateLastActiveTime();

    if (is_complete_)
        return Result<void>();

    // タイムアウトやエラーは全状態で共通してセッション終了
    if (event.type == kTimeoutEvent || event.type == kErrorEvent)
    {
        // 強制的にクローズ待機状態へ
        changeState(new CloseWaitState());
        // 即時反映
        if (current_state_)
            delete current_state_;
        current_state_ = pending_state_;
        pending_state_ = NULL;
        return current_state_->handleEvent(*this, event);
    }

    // それ以外のイベントは現在の状態に委譲
    Result<void> result = Result<void>();
    if (current_state_)
    {
        result = current_state_->handleEvent(*this, event);
    }

    // 状態遷移があれば反映
    if (pending_state_ != NULL)
    {
        if (current_state_)
            delete current_state_;
        current_state_ = pending_state_;
        pending_state_ = NULL;
    }

    return result;
}

}  // namespace server
