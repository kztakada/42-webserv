#ifndef WEBSERV_REQUEST_DISPATCHER_HPP_
#define WEBSERV_REQUEST_DISPATCHER_HPP_

#include "utils/result.hpp"

namespace server
{

struct SessionContext;
class RequestRouter;
class IRequestAction;
class IoBuffer;

class RequestDispatcher
{
   public:
    explicit RequestDispatcher(const RequestRouter& router);

    utils::result::Result<void> consumeFromRecvBuffer(SessionContext& ctx);
    utils::result::Result<IRequestAction*> dispatch(SessionContext& ctx);

   private:
    const RequestRouter& router_;
    utils::result::Result<void> finalizeUploadStoreIfNeeded_(
        SessionContext& ctx);
};

}  // namespace server
#endif