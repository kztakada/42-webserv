#include "server/http_processing_module/request_processor/action_handler_factory.hpp"

#include "server/http_processing_module/request_processor/handlers/internal_redirect_handler.hpp"
#include "server/http_processing_module/request_processor/handlers/redirect_external_handler.hpp"
#include "server/http_processing_module/request_processor/handlers/respond_error_handler.hpp"
#include "server/http_processing_module/request_processor/handlers/static_autoindex_handler.hpp"
#include "server/http_processing_module/request_processor/handlers/store_body_handler.hpp"

namespace server
{

ActionHandlerFactory::ActionHandlerFactory(const RequestRouter& router,
    AutoIndexRenderer& autoindex_renderer, ErrorPageRenderer& error_renderer,
    InternalRedirectResolver& internal_redirect,
    StaticFileResponder& file_responder)
    : internal_redirect_handler_(new InternalRedirectHandler()),
      redirect_external_handler_(new RedirectExternalHandler()),
      respond_error_handler_(new RespondErrorHandler(error_renderer)),
      store_body_handler_(new StoreBodyHandler(error_renderer)),
      static_autoindex_handler_(
          new StaticAutoIndexHandler(router, autoindex_renderer, error_renderer,
              internal_redirect, file_responder))
{
}

ActionHandler* ActionHandlerFactory::getHandler(ActionType action)
{
    switch (action)
    {
        case REDIRECT_INTERNAL:
            return internal_redirect_handler_;
        case REDIRECT_EXTERNAL:
            return redirect_external_handler_;
        case RESPOND_ERROR:
            return respond_error_handler_;
        case STORE_BODY:
            return store_body_handler_;
        case SERVE_STATIC:
        case SERVE_AUTOINDEX:
            return static_autoindex_handler_;
        default:
            return NULL;
    }
}

ActionHandlerFactory::~ActionHandlerFactory()
{
    delete internal_redirect_handler_;
    delete redirect_external_handler_;
    delete respond_error_handler_;
    delete store_body_handler_;
    delete static_autoindex_handler_;
}

}  // namespace server
