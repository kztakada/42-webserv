#include "http/cgi_meta_variables.hpp"

#include <cctype>
#include <sstream>

#include "http/header.hpp"

namespace http
{

CgiMetaVariables::CgiMetaVariables() : variables_()
{
    setGatewayInterface("CGI/1.1");
}

void CgiMetaVariables::set(const std::string& name, const std::string& value)
{
    variables_[name] = value;
}

void CgiMetaVariables::setAuthType(const std::string& auth_type)
{
    set("AUTH_TYPE", auth_type);
}

void CgiMetaVariables::setContentLength(unsigned long content_length)
{
    std::ostringstream oss;
    oss << content_length;
    set("CONTENT_LENGTH", oss.str());
}

void CgiMetaVariables::setContentType(const std::string& content_type)
{
    set("CONTENT_TYPE", content_type);
}

void CgiMetaVariables::setGatewayInterface(const std::string& version)
{
    set("GATEWAY_INTERFACE", version);
}

void CgiMetaVariables::setPathInfo(const std::string& path_info)
{
    set("PATH_INFO", path_info);
}

void CgiMetaVariables::setPathTranslated(const std::string& path_translated)
{
    set("PATH_TRANSLATED", path_translated);
}

void CgiMetaVariables::setQueryString(const std::string& query_string)
{
    set("QUERY_STRING", query_string);
}

void CgiMetaVariables::setRemoteAddr(const std::string& remote_addr)
{
    set("REMOTE_ADDR", remote_addr);
}

void CgiMetaVariables::setRemoteHost(const std::string& remote_host)
{
    set("REMOTE_HOST", remote_host);
}

void CgiMetaVariables::setRemoteIdent(const std::string& remote_ident)
{
    set("REMOTE_IDENT", remote_ident);
}

void CgiMetaVariables::setRemoteUser(const std::string& remote_user)
{
    set("REMOTE_USER", remote_user);
}

void CgiMetaVariables::setRequestMethod(const std::string& method)
{
    set("REQUEST_METHOD", method);
}

void CgiMetaVariables::setScriptName(const std::string& script_name)
{
    set("SCRIPT_NAME", script_name);
}

void CgiMetaVariables::setServerName(const std::string& server_name)
{
    set("SERVER_NAME", server_name);
}

void CgiMetaVariables::setServerPort(unsigned int server_port)
{
    std::ostringstream oss;
    oss << server_port;
    set("SERVER_PORT", oss.str());
}

void CgiMetaVariables::setServerProtocol(const std::string& protocol)
{
    set("SERVER_PROTOCOL", protocol);
}

void CgiMetaVariables::setServerSoftware(const std::string& software)
{
    set("SERVER_SOFTWARE", software);
}

std::string CgiMetaVariables::convertHeaderNameToCgiFormat(
    const std::string& header_name)
{
    std::string out;
    out.reserve(header_name.size());

    for (size_t i = 0; i < header_name.size(); ++i)
    {
        char c = header_name[i];
        if (c == '-')
            c = '_';
        out.push_back(
            static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

void CgiMetaVariables::setHttpHeader(
    const std::string& header_name, const std::string& value)
{
    const std::string name = convertHeaderNameToCgiFormat(header_name);

    // CONTENT_LENGTH/CONTENT_TYPE は専用変数として扱う
    if (name == "CONTENT_LENGTH" || name == "CONTENT_TYPE")
        return;

    set("HTTP_" + name, value);
}

const std::map<std::string, std::string>& CgiMetaVariables::getAll() const
{
    return variables_;
}

CgiMetaVariables CgiMetaVariables::fromHttpRequest(const HttpRequest& request,
    const std::string& script_name, const std::string& path_info)
{
    CgiMetaVariables v;

    v.setRequestMethod(request.getMethodString());
    v.setScriptName(script_name);
    v.setPathInfo(path_info);
    v.setQueryString(request.getQueryString());

    // SERVER_PROTOCOL 例: HTTP/1.1
    v.setServerProtocol(request.getHttpVersion());

    // Host/Port 等は上位（セッション/ルーティング）側で埋めるのが正確だが、
    // ここでは最小限。

    // Content-* の抽出
    if (request.hasHeader(HeaderName(HeaderName::CONTENT_TYPE).toString()))
    {
        Result<const std::vector<std::string>&> ct =
            request.getHeader(HeaderName(HeaderName::CONTENT_TYPE).toString());
        if (ct.isOk() && !ct.unwrap().empty())
            v.setContentType(ct.unwrap()[0]);
    }

    if (request.hasHeader(HeaderName(HeaderName::CONTENT_LENGTH).toString()))
    {
        Result<const std::vector<std::string>&> cl = request.getHeader(
            HeaderName(HeaderName::CONTENT_LENGTH).toString());
        if (cl.isOk() && !cl.unwrap().empty())
        {
            std::istringstream iss(cl.unwrap()[0]);
            unsigned long n = 0;
            iss >> n;
            v.setContentLength(n);
        }
    }

    // HTTP_*
    const HeaderMap& h = request.getHeaders();
    for (HeaderMap::const_iterator it = h.begin(); it != h.end(); ++it)
    {
        const std::string& name = it->first;
        const std::vector<std::string>& values = it->second;
        if (values.empty())
            continue;
        // 複数値の結合
        // - Cookie は RFC 6265 に寄せて "; " で結合
        // - その他はカンマ結合（簡易）
        const bool is_cookie =
            HeaderName::fromString(name) == HeaderName::COOKIE;
        const std::string sep = is_cookie ? "; " : ",";
        std::string merged = values[0];
        for (size_t i = 1; i < values.size(); ++i)
        {
            merged += sep;
            merged += values[i];
        }
        v.setHttpHeader(name, merged);
    }

    return v;
}

}  // namespace http
