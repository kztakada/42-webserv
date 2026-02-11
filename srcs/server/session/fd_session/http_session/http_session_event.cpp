#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::handleEvent(const FdEvent& event)
{
    updateLastActiveTime();

    if (context_.is_complete)
        return Result<void>();

    // タイムアウトやエラーは全状態で共通してセッション終了
    if (event.type == kTimeoutEvent || event.type == kErrorEvent)
    {
        // 親が先に delete されると、CgiSession 側の parent_session_ 参照が
        // ダングリングになり UAF になり得るため、エラー時は必ず CGI
        // も回収する。
        cleanupCgiOnClose_();

        // 強制的にクローズ待機状態へ
        changeState(new CloseWaitState());
        // 即時反映
        if (context_.current_state)
            delete context_.current_state;
        context_.current_state = context_.pending_state;
        context_.pending_state = NULL;
        return context_.current_state->handleEvent(*this, event);
    }

    // それ以外のイベントは現在の状態に委譲
    Result<void> result = Result<void>();
    if (context_.current_state)
    {
        result = context_.current_state->handleEvent(*this, event);
    }

    // 状態遷移があれば反映
    if (context_.pending_state != NULL)
    {
        if (context_.current_state)
            delete context_.current_state;
        context_.current_state = context_.pending_state;
        context_.pending_state = NULL;
    }

    return result;
}

}  // namespace server
