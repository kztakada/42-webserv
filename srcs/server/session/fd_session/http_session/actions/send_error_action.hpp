#ifndef WEBSERV_SEND_ERROR_ACTION_HPP_
#define WEBSERV_SEND_ERROR_ACTION_HPP_

#include "server/session/fd_session/http_session/actions/i_request_action.hpp"
#include "http/status.hpp"

namespace server {

class SendErrorAction : public IRequestAction {
public:
    explicit SendErrorAction(http::HttpStatus status);
    virtual utils::result::Result<void> execute(HttpSession& session);
private:
    http::HttpStatus status_;
};

}
#endif
