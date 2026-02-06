#include "server/http_processing_module/request_processor/autoindex_renderer.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>

#include "server/http_processing_module/request_processor/config_text_loader.hpp"

namespace server
{

using utils::result::Result;

std::string AutoIndexRenderer::htmlEscape_(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (std::string::size_type i = 0; i < s.size(); ++i)
    {
        const char c = s[i];
        if (c == '&')
            out += "&amp;";
        else if (c == '<')
            out += "&lt;";
        else if (c == '>')
            out += "&gt;";
        else if (c == '"')
            out += "&quot;";
        else
            out.push_back(c);
    }
    return out;
}

bool AutoIndexRenderer::isUnreservedUriChar_(unsigned char c)
{
    if (c >= 'a' && c <= 'z')
        return true;
    if (c >= 'A' && c <= 'Z')
        return true;
    if (c >= '0' && c <= '9')
        return true;
    if (c == '-' || c == '.' || c == '_' || c == '~')
        return true;
    return false;
}

std::string AutoIndexRenderer::percentEncodeUriComponent_(const std::string& s)
{
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    for (std::string::size_type i = 0; i < s.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (isUnreservedUriChar_(c))
        {
            out.push_back(static_cast<char>(c));
        }
        else
        {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

std::string AutoIndexRenderer::parentUriPath_(const std::string& uri_dir_path)
{
    if (uri_dir_path.empty() || uri_dir_path[0] != '/')
        return std::string("/");
    if (uri_dir_path == "/")
        return std::string("/");

    std::string p = uri_dir_path;
    while (!p.empty() && p[p.size() - 1] == '/')
        p.erase(p.size() - 1);
    const std::string::size_type last = p.find_last_of('/');
    if (last == std::string::npos)
        return std::string("/");
    if (last == 0)
        return std::string("/");
    return p.substr(0, last + 1);
}

void AutoIndexRenderer::replaceAll_(
    std::string* inout, const std::string& from, const std::string& to)
{
    if (inout == NULL)
        return;
    if (from.empty())
        return;

    std::string& s = *inout;
    std::string::size_type pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string AutoIndexRenderer::renderTemplate_(const std::string& tmpl,
    const std::vector<std::pair<std::string, std::string> >& replacements)
{
    std::string out = tmpl;
    for (size_t i = 0; i < replacements.size(); ++i)
    {
        replaceAll_(&out, replacements[i].first, replacements[i].second);
    }
    return out;
}

std::string AutoIndexRenderer::loadAutoIndexCss_()
{
    Result<std::string> r = ConfigTextLoader::load("autoindex.css");
    if (r.isOk())
        return r.unwrap();
    return std::string();
}

std::string AutoIndexRenderer::loadAutoIndexTemplate_()
{
    Result<std::string> r = ConfigTextLoader::load("autoindex.html");
    if (r.isOk())
        return r.unwrap();
    return std::string();
}

std::string AutoIndexRenderer::loadAutoIndexEntryTemplate_()
{
    Result<std::string> r = ConfigTextLoader::load("autoindex_entry.html");
    if (r.isOk())
        return r.unwrap();
    return std::string();
}

std::string AutoIndexRenderer::loadAutoIndexParentEntryTemplate_()
{
    Result<std::string> r =
        ConfigTextLoader::load("autoindex_parent_entry.html");
    if (r.isOk())
        return r.unwrap();
    return std::string();
}

Result<std::string> AutoIndexRenderer::buildBody(
    const AutoIndexContext& ctx) const
{
    const std::string dir = ctx.directory_path.str();
    DIR* dp = ::opendir(dir.c_str());
    if (dp == NULL)
        return Result<std::string>(ERROR, std::string(), "opendir failed");

    std::vector<std::string> entries;
    for (;;)
    {
        struct dirent* de = ::readdir(dp);
        if (de == NULL)
            break;
        const std::string name(de->d_name ? de->d_name : "");
        if (name.empty())
            continue;
        if (name == "." || name == "..")
            continue;
        entries.push_back(name);
    }
    ::closedir(dp);

    std::sort(entries.begin(), entries.end());

    std::string uri = ctx.uri_dir_path;
    if (uri.empty() || uri[0] != '/')
        uri = "/";
    if (uri[uri.size() - 1] != '/')
        uri += "/";

    const std::string css = loadAutoIndexCss_();
    const std::string tmpl = loadAutoIndexTemplate_();
    const std::string entry_tmpl = loadAutoIndexEntryTemplate_();
    const std::string parent_entry_tmpl = loadAutoIndexParentEntryTemplate_();

    if (css.empty() || tmpl.empty() || entry_tmpl.empty() ||
        parent_entry_tmpl.empty())
    {
        return Result<std::string>(
            ERROR, std::string(), "autoindex template/css missing");
    }

    std::string entries_html;
    if (uri != "/")
    {
        const std::string parent = parentUriPath_(uri);
        std::string row = parent_entry_tmpl;
        std::vector<std::pair<std::string, std::string> > repl;
        repl.push_back(std::make_pair("{{HREF}}", htmlEscape_(parent)));
        entries_html += renderTemplate_(row, repl);
    }

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const std::string& name = entries[i];
        const std::string entry_physical = dir + "/" + name;
        struct stat st;
        bool is_dir = false;
        if (::stat(entry_physical.c_str(), &st) == 0)
            is_dir = S_ISDIR(st.st_mode);

        std::string href = uri + percentEncodeUriComponent_(name);
        std::string label = name;
        if (is_dir)
        {
            href += "/";
            label += "/";
        }

        std::string row = entry_tmpl;
        std::vector<std::pair<std::string, std::string> > repl;
        repl.push_back(std::make_pair("{{HREF}}", htmlEscape_(href)));
        repl.push_back(std::make_pair("{{LABEL}}", htmlEscape_(label)));
        entries_html += renderTemplate_(row, repl);
    }

    const std::string title = std::string("Index of ") + uri;

    std::vector<std::pair<std::string, std::string> > replacements;
    replacements.push_back(std::make_pair("{{CSS}}", css));
    replacements.push_back(std::make_pair("{{TITLE}}", htmlEscape_(title)));
    replacements.push_back(std::make_pair("{{PATH}}", htmlEscape_(uri)));
    replacements.push_back(std::make_pair("{{ENTRIES}}", entries_html));
    return renderTemplate_(tmpl, replacements);
}

}  // namespace server
