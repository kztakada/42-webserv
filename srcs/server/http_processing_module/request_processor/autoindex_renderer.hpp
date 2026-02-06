#ifndef WEBSERV_AUTOINDEX_RENDERER_HPP_
#define WEBSERV_AUTOINDEX_RENDERER_HPP_

#include <string>
#include <utility>
#include <vector>

#include "server/http_processing_module/request_router/location_routing.hpp"
#include "utils/result.hpp"

namespace server
{

class AutoIndexRenderer
{
   public:
    utils::result::Result<std::string> buildBody(
        const AutoIndexContext& ctx) const;

   private:
    static std::string htmlEscape_(const std::string& s);
    static bool isUnreservedUriChar_(unsigned char c);
    static std::string percentEncodeUriComponent_(const std::string& s);
    static std::string parentUriPath_(const std::string& uri_dir_path);
    static void replaceAll_(
        std::string* inout, const std::string& from, const std::string& to);
    static std::string renderTemplate_(const std::string& tmpl,
        const std::vector<std::pair<std::string, std::string> >& replacements);
    static std::string loadAutoIndexCss_();
    static std::string loadAutoIndexTemplate_();
    static std::string loadAutoIndexEntryTemplate_();
    static std::string loadAutoIndexParentEntryTemplate_();
};

}  // namespace server

#endif
