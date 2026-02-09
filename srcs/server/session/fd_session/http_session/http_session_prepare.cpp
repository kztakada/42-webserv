#include <string>

#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/actions/send_error_action.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "utils/log.hpp"

namespace server
{

using namespace utils::result;

Result<void> HttpSession::prepareResponseOrCgi_()
{
    context_.response.reset();

    Result<IRequestAction*> action_r = module_.dispatcher.dispatch(context_);
    if (action_r.isError())
    {
        // ディスパッチ失敗
        return Result<void>(ERROR, action_r.getErrorMessage());
    }

    IRequestAction* action = action_r.unwrap();
    Result<void> r = action->execute(*this);
    delete action;
    return r;
}

Result<void> HttpSession::consumeRecvBufferWithoutRead_()
{
    const bool was_parse_complete = context_.request.isParseComplete();

    for (;;)
    {
        const size_t before = context_.recv_buffer.size();

        Result<void> c = module_.dispatcher.consumeFromRecvBuffer(context_);
        if (c.isError())
        {
            http::HttpStatus st = context_.request.getParseErrorStatus();
            if (st == http::HttpStatus::OK)
                st = http::HttpStatus::BAD_REQUEST;

            SendErrorAction action(st);
            return action.execute(*this);
        }

        // ログ出力のための実装
        if (!was_parse_complete && context_.request.isParseComplete())
        {
            std::string request_host =
                context_.socket_fd.getServerIp().toString() + ":" +
                context_.socket_fd.getServerPort().toString();
            Result<const std::vector<std::string>&> host_header =
                context_.request.getHeader("Host");
            if (host_header.isOk())
            {
                const std::vector<std::string>& values = host_header.unwrap();
                if (!values.empty() && !values[0].empty() &&
                    request_host != values[0])
                    request_host = request_host + "(" + values[0] + ")";
            }

            // infoログ
            const std::string info_msg =
                std::string("Host: ") + request_host + " Accepted request " +
                context_.request.getMethod().toString() + " " +
                context_.request.getPath() + " from " +
                context_.socket_fd.getClientIp().toString() + ":" +
                context_.socket_fd.getClientPort().toString();
            utils::Log::info(info_msg);

            // debugログ
            const std::string& query = context_.request.getQueryString();
            const std::string target =
                query.empty() ? context_.request.getPath()
                              : (context_.request.getPath() + "?" + query);
            const std::string debug_msg =
                std::string("Host: ") + request_host + " Accepted request " +
                context_.request.getMethod().toString() + " " +
                context_.request.getPath() + " from " +
                context_.socket_fd.getClientIp().toString() + ":" +
                context_.socket_fd.getClientPort().toString() + " " + target;
            utils::Log::debug(debug_msg);
        }

        if (context_.request.isParseComplete())
            return prepareResponseOrCgi_();

        const size_t after = context_.recv_buffer.size();

        if (after >= before)
            break;
    }
    return Result<void>();
}

}  // namespace server
