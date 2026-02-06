#ifndef WEBSERV_REQUEST_DISPATCHER_HPP_
#define WEBSERV_REQUEST_DISPATCHER_HPP_

#include "utils/result.hpp"

namespace server {

struct SessionContext;
class IRequestAction;
class IoBuffer;

class RequestDispatcher {
public:
    RequestDispatcher();
    
    utils::result::Result<void> consumeFromRecvBuffer(SessionContext& ctx);
    utils::result::Result<IRequestAction*> dispatch(SessionContext& ctx);

private:
    utils::result::Result<void> finalizeUploadStoreIfNeeded_(SessionContext& ctx);
};

}
#endif