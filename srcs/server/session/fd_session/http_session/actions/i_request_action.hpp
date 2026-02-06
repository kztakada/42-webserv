#ifndef WEBSERV_I_REQUEST_ACTION_HPP_
#define WEBSERV_I_REQUEST_ACTION_HPP_

#include "utils/result.hpp"

namespace server {

class HttpSession;

class IRequestAction {
public:
    virtual ~IRequestAction() {}
    virtual utils::result::Result<void> execute(HttpSession& session) = 0;
};

} // namespace server

#endif
