#ifndef WEBSERV_CONFIG_TEXT_LOADER_HPP_
#define WEBSERV_CONFIG_TEXT_LOADER_HPP_

#include <string>

#include "utils/result.hpp"

namespace server
{

class ConfigTextLoader
{
   public:
    static utils::result::Result<std::string> load(
        const std::string& file_name);

   private:
    static utils::result::Result<std::string> readFileToString_(
        const std::string& path);
};

}  // namespace server

#endif
