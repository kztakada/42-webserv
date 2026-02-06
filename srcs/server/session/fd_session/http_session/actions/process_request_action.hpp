#ifndef WEBSERV_PROCESS_REQUEST_ACTION_HPP_
#define WEBSERV_PROCESS_REQUEST_ACTION_HPP_

#include "server/session/fd_session/http_session/actions/i_request_action.hpp"

namespace server {

class ProcessRequestAction : public IRequestAction {
public:
    virtual utils::result::Result<void> execute(HttpSession& session);
};

}
#endif
