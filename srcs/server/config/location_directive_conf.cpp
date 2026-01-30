#include "server/config/location_directive_conf.hpp"

#include "utils/path.hpp"
#include "utils/result.hpp"

namespace server
{

using utils::path::PhysicalPath;
using utils::result::ERROR;
using utils::result::Result;
static bool containsNul_(const std::string& s)
{
    return s.find('\0') != std::string::npos;
}

static Result<void> validateNonEmptyToken_(
    const std::string& token, const char* empty_msg, const char* nul_msg)
{
    if (token.empty())
    {
        return Result<void>(ERROR, empty_msg);
    }
    if (containsNul_(token))
    {
        return Result<void>(ERROR, nul_msg);
    }
    return Result<void>();
}

static bool isValidRedirectTarget_(const std::string& target)
{
    if (target.empty() || containsNul_(target))
    {
        return false;
    }
    if (target[0] == '/')
    {
        return true;
    }
    if (target.compare(0, 7, "http://") == 0)
    {
        return true;
    }
    if (target.compare(0, 8, "https://") == 0)
    {
        return true;
    }
    return false;
}

static bool isValidUriOrUrlTarget_(const std::string& target)
{
    return isValidRedirectTarget_(target);
}

LocationDirectiveConf::LocationDirectiveConf()
    : is_backward_search(false),
      has_allowed_methods(false),
      client_max_body_size(1024 * 1024),
      has_client_max_body_size(false),
      has_root_dir(false),
      has_index_pages(false),
      has_error_pages(false),
      auto_index(false),
      has_auto_index(false),
      redirect_status(http::HttpStatus::UNKNOWN)
{
}

Result<void> LocationDirectiveConf::setPathPattern(
    const std::string& path_pattern_str)
{
    if (path_pattern_str.empty() || path_pattern_str[0] != '/')
    {
        return Result<void>(ERROR, "location path must start with '/'");
    }
    if (containsNul_(path_pattern_str))
    {
        return Result<void>(ERROR, "location path contains NUL");
    }
    this->path_pattern = path_pattern_str;
    return Result<void>();
}

Result<void> LocationDirectiveConf::setIsBackwardSearch(bool is_backward_search)
{
    this->is_backward_search = is_backward_search;
    return Result<void>();
}

Result<void> LocationDirectiveConf::appendAllowedMethod(
    const http::HttpMethod& method)
{
    if (!(method == http::HttpMethod::GET || method == http::HttpMethod::POST ||
            method == http::HttpMethod::DELETE))
    {
        return Result<void>(
            ERROR, "allow_methods supports GET/POST/DELETE only");
    }
    has_allowed_methods = true;
    allowed_methods.insert(method);
    return Result<void>();
}

Result<void> LocationDirectiveConf::setClientMaxBodySize(unsigned long size)
{
    if (has_client_max_body_size)
    {
        return Result<void>(
            ERROR, "directive is duplicate: client_max_body_size");
    }
    if (size > static_cast<unsigned long>(INT_MAX))
    {
        return Result<void>(ERROR, "client_max_body_size exceeds INT_MAX");
    }
    has_client_max_body_size = true;
    client_max_body_size = size;
    return Result<void>();
}

Result<void> LocationDirectiveConf::setRootDir(const std::string& root_dir_str)
{
    if (has_root_dir)
    {
        return Result<void>(ERROR, "directive is duplicate: root");
    }
    Result<void> v = validateNonEmptyToken_(
        root_dir_str, "root path is empty", "root path contains NUL");
    if (v.isError())
    {
        return v;
    }
    Result<PhysicalPath> resolved = PhysicalPath::resolve(root_dir_str);
    if (resolved.isError())
    {
        return Result<void>(ERROR, "root path is invalid");
    }
    has_root_dir = true;
    this->root_dir = resolved.unwrap();
    return Result<void>();
}

Result<void> LocationDirectiveConf::appendIndexPage(
    const std::string& filename_str)
{
    Result<void> v = validateNonEmptyToken_(filename_str,
        "index page path is empty", "index page path contains NUL");
    if (v.isError())
    {
        return v;
    }
    has_index_pages = true;
    index_pages.push_back(filename_str);
    return Result<void>();
}

Result<void> LocationDirectiveConf::appendCgiExtension(
    const std::string& cgi_ext_str, const std::string& cgi_path_str)
{
    Result<void> ext_v = validateNonEmptyToken_(cgi_ext_str,
        "cgi_extension ext is empty", "cgi_extension ext contains NUL");
    if (ext_v.isError())
    {
        return ext_v;
    }
    if (cgi_ext_str[0] != '.')
    {
        return Result<void>(ERROR, "cgi_extension ext must start with '.'");
    }
    Result<void> path_v = validateNonEmptyToken_(cgi_path_str,
        "cgi_extension path is empty", "cgi_extension path contains NUL");
    if (path_v.isError())
    {
        return path_v;
    }
    Result<PhysicalPath> resolved = PhysicalPath::resolve(cgi_path_str);
    if (resolved.isError())
    {
        return Result<void>(ERROR, "cgi_extension path is invalid");
    }
    cgi_extensions[cgi_ext_str] = resolved.unwrap();
    return Result<void>();
}

Result<void> LocationDirectiveConf::appendErrorPage(
    http::HttpStatus status, const std::string& page_url_str)
{
    if (!status.isError())
    {
        return Result<void>(ERROR, "error_page status must be 4xx/5xx");
    }
    Result<void> v = validateNonEmptyToken_(page_url_str,
        "error_page path is empty", "error_page path contains NUL");
    if (v.isError())
    {
        return v;
    }
    if (!isValidUriOrUrlTarget_(page_url_str))
    {
        return Result<void>(ERROR, "error_page path is invalid");
    }
    has_error_pages = true;
    error_pages[status] = page_url_str;
    return Result<void>();
}

Result<void> LocationDirectiveConf::setAutoIndex(bool auto_index)
{
    if (has_auto_index)
    {
        return Result<void>(ERROR, "directive is duplicate: autoindex");
    }
    has_auto_index = true;
    this->auto_index = auto_index;
    return Result<void>();
}

Result<void> LocationDirectiveConf::setRedirect(
    http::HttpStatus status, const std::string& redirect_url_str)
{
    if (!this->redirect_url.empty() ||
        this->redirect_status != http::HttpStatus::UNKNOWN)
    {
        return Result<void>(ERROR, "directive is duplicate: return");
    }
    if (!isValidRedirectTarget_(redirect_url_str))
    {
        if (redirect_url_str.empty())
        {
            return Result<void>(ERROR, "return url is empty");
        }
        if (containsNul_(redirect_url_str))
        {
            return Result<void>(ERROR, "return url contains NUL");
        }
        return Result<void>(ERROR, "return url is invalid");
    }
    if (!status.isRedirection())
    {
        return Result<void>(ERROR, "return code must be 3xx");
    }
    redirect_status = status;
    this->redirect_url = redirect_url_str;
    return Result<void>();
}

Result<void> LocationDirectiveConf::setUploadStore(
    const std::string& upload_store_str)
{
    if (!this->upload_store.empty())
    {
        return Result<void>(ERROR, "directive is duplicate: upload_store");
    }
    Result<void> v = validateNonEmptyToken_(upload_store_str,
        "upload_store path is empty", "upload_store path contains NUL");
    if (v.isError())
    {
        return v;
    }
    Result<PhysicalPath> resolved = PhysicalPath::resolve(upload_store_str);
    if (resolved.isError())
    {
        return Result<void>(ERROR, "upload_store path is invalid");
    }
    this->upload_store = resolved.unwrap();
    return Result<void>();
}

bool LocationDirectiveConf::isValid() const
{
    if (client_max_body_size > INT_MAX)
    {
        return false;
    }

    for (CgiExtensionsMap::const_iterator it = cgi_extensions.begin();
        it != cgi_extensions.end(); ++it)
    {
        if (it->first.empty() || it->second.empty())
        {
            return false;
        }
    }

    // return は "code url" の2引数で指定される想定。
    // urlだけ/codeだけ などの不整合は invalid とする。
    if (redirect_url.empty() != (redirect_status == http::HttpStatus::UNKNOWN))
    {
        return false;
    }
    if (!redirect_url.empty() && !redirect_status.isRedirection())
    {
        return false;
    }
    return true;
}

}  // namespace server
