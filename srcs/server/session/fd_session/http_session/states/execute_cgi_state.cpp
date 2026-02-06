#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{
using namespace utils::result;

Result<void> ExecuteCgiState::handleEvent(
    HttpSession& context, const FdEvent& event)
{
    if (event.fd == context.context_.socket_fd.getFd() &&
        event.type == kReadEvent)
    {
        if (context.context_.recv_buffer.size() <
            HttpSession::kMaxRecvBufferBytes)
        {
            const ssize_t n = context.context_.recv_buffer.fillFromFd(
                context.context_.socket_fd.getFd());
            if (n == 0)
            {
                context.context_.peer_closed = true;
                context.context_.should_close_connection = true;
            }
        }
        context.updateSocketWatches_();
    }

    if (event.is_opposite_close &&
        event.fd == context.context_.socket_fd.getFd())
    {
        context.context_.peer_closed = true;
        context.context_.should_close_connection = true;
    }

    if (context.context_.peer_closed)
    {
        context.changeState(new CloseWaitState());
        context.context_.socket_fd.shutdown();

        if (context.getContext().active_cgi_session != NULL)
        {
            context.controller_.requestDelete(
                context.getContext().active_cgi_session);
            context.getContext().active_cgi_session = NULL;
        }
        context.controller_.requestDelete(&context);
    }
    return Result<void>();
}

void ExecuteCgiState::getWatchFlags(
    const HttpSession& session, bool* want_read, bool* want_write) const
{
    if (want_read)
    {
        *want_read = (session.context_.recv_buffer.size() <
                      HttpSession::kMaxRecvBufferBytes);
    }
    if (want_write)
    {
        *want_write = false;
    }
}

}  // namespace server