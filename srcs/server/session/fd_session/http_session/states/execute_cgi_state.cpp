#include "server/session/fd_session/http_session/states/http_session_states.hpp"
#include "server/session/fd_session/http_session.hpp"
#include "server/session/fd_session/cgi_session.hpp"
#include "server/session/fd_session_controller.hpp"

namespace server
{
using namespace utils::result;

Result<void> ExecuteCgiState::handleEvent(HttpSession& context, const FdEvent& event) {
    if (event.fd == context.socket_fd_.getFd() && event.type == kReadEvent) {
        if (context.recv_buffer_.size() < HttpSession::kMaxRecvBufferBytes) {
            const ssize_t n = context.recv_buffer_.fillFromFd(context.socket_fd_.getFd());
            if (n == 0) {
                context.peer_closed_ = true;
                context.should_close_connection_ = true;
            }
        }
        context.updateSocketWatches_();
    }
    
    if (event.is_opposite_close && event.fd == context.socket_fd_.getFd()) {
         context.peer_closed_ = true;
         context.should_close_connection_ = true;
    }

    if (context.peer_closed_) {
        context.changeState(new CloseWaitState());
        context.socket_fd_.shutdown();
        
        if (context.active_cgi_session_ != NULL) {
            context.controller_.requestDelete(context.active_cgi_session_);
            context.active_cgi_session_ = NULL;
        }
        context.controller_.requestDelete(&context);
    }
    return Result<void>();
}

}  // namespace server
