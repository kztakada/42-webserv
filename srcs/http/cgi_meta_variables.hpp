#ifndef HTTP_CGI_META_VARIABLES_HPP_
#define HTTP_CGI_META_VARIABLES_HPP_

#include <map>
#include <string>

#include "http/http_request.hpp"

namespace http
{

// RFC 3875 Section 4で定義されたCGIメタ変数
class CgiMetaVariables
{
   public:
    CgiMetaVariables();

    // RFC 3875 Section 4.1: Request Meta-Variables
    void setAuthType(const std::string& auth_type);
    void setContentLength(unsigned long content_length);
    void setContentType(const std::string& content_type);
    void setGatewayInterface(const std::string& version);  // Default: "CGI/1.1"
    void setPathInfo(const std::string& path_info);
    void setPathTranslated(const std::string& path_translated);
    void setQueryString(const std::string& query_string);
    void setRemoteAddr(const std::string& remote_addr);
    void setRemoteHost(const std::string& remote_host);
    void setRemoteIdent(const std::string& remote_ident);
    void setRemoteUser(const std::string& remote_user);
    void setRequestMethod(const std::string& method);
    void setScriptName(const std::string& script_name);
    void setServerName(const std::string& server_name);
    void setServerPort(unsigned int server_port);
    void setServerProtocol(const std::string& protocol);  // e.g., "HTTP/1.1"
    void setServerSoftware(const std::string& software);

    // HTTP_* 変数（RFC 3875 Section 4.1.18）
    void setHttpHeader(
        const std::string& header_name, const std::string& value);

    // すべての変数を取得
    const std::map<std::string, std::string>& getAll() const;

    // HTTPリクエストから自動生成
    static CgiMetaVariables fromHttpRequest(const HttpRequest& request,
        const std::string& script_name, const std::string& path_info = "");

   private:
    std::map<std::string, std::string> variables_;

    void set(const std::string& name, const std::string& value);
    static std::string convertHeaderNameToCgiFormat(
        const std::string& header_name);
};

}  // namespace http

#endif