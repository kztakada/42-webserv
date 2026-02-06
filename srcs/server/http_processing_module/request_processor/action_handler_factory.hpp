#ifndef WEBSERV_ACTION_HANDLER_FACTORY_HPP_
#define WEBSERV_ACTION_HANDLER_FACTORY_HPP_

#include "server/http_processing_module/request_processor/action_handler.hpp"

namespace server
{

class AutoIndexRenderer;
class ErrorPageRenderer;
class InternalRedirectResolver;
class StaticFileResponder;
class RequestRouter;

class ActionHandlerFactory
{
   public:
    ActionHandlerFactory(const RequestRouter& router,
        AutoIndexRenderer& autoindex_renderer,
        ErrorPageRenderer& error_renderer,
        InternalRedirectResolver& internal_redirect,
        StaticFileResponder& file_responder);

    ActionHandler* getHandler(ActionType action);

    ~ActionHandlerFactory();

   private:
    ActionHandlerFactory();
    ActionHandlerFactory(const ActionHandlerFactory& rhs);
    ActionHandlerFactory& operator=(const ActionHandlerFactory& rhs);

    ActionHandler* internal_redirect_handler_;
    ActionHandler* redirect_external_handler_;
    ActionHandler* respond_error_handler_;
    ActionHandler* store_body_handler_;
    ActionHandler* static_autoindex_handler_;
};

}  // namespace server

#endif
