#include "server/http_processing_module/request_processor/error_page_renderer.hpp"

#include <sstream>
#include <utility>
#include <vector>

#include "server/http_processing_module/request_processor/config_text_loader.hpp"
#include "server/session/fd_session/http_session/body_source.hpp"

namespace server
{

using utils::result::Result;

std::string ErrorPageRenderer::htmlEscape_(const std::string& s)
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

std::string ErrorPageRenderer::buildDefaultErrorPageBody_(
    const http::HttpStatus& status) const
{
    std::string css;
    {
        Result<std::string> r = ConfigTextLoader::load("error_page.css");
        if (r.isOk())
            css = r.unwrap();
    }
    std::string tmpl;
    {
        Result<std::string> r = ConfigTextLoader::load("error_page.html");
        if (r.isOk())
            tmpl = r.unwrap();
    }

    std::ostringstream oss;
    oss << status.getCode() << " " << status.getMessage();
    const std::string status_line = oss.str();

    std::ostringstream code_oss;
    code_oss << status.getCode();
    const std::string code_str = code_oss.str();

    if (tmpl.empty())
    {
        // 最終フォールバック（HTMLテンプレが無い場合でも何か返す）
        return status_line + "\n";
    }

    std::vector<std::pair<std::string, std::string> > replacements;
    replacements.push_back(std::make_pair("{{CSS}}", css));
    replacements.push_back(std::make_pair("{{CODE}}", htmlEscape_(code_str)));
    replacements.push_back(
        std::make_pair("{{STATUS_LINE}}", htmlEscape_(status_line)));
    replacements.push_back(
        std::make_pair("{{MESSAGE}}", htmlEscape_(status.getMessage())));

    // 簡易テンプレート置換
    std::string out = tmpl;
    for (size_t i = 0; i < replacements.size(); ++i)
    {
        const std::string& from = replacements[i].first;
        const std::string& to = replacements[i].second;
        std::string::size_type pos = 0;
        while ((pos = out.find(from, pos)) != std::string::npos)
        {
            out.replace(pos, from.size(), to);
            pos += to.size();
        }
    }
    return out;
}

Result<RequestProcessorOutput> ErrorPageRenderer::respond(
    const http::HttpStatus& status, http::HttpResponse& out_response) const
{
    RequestProcessorOutput out;

    Result<void> s = out_response.setStatus(status);
    if (s.isError())
        return Result<RequestProcessorOutput>(ERROR, s.getErrorMessage());

    const std::string body = buildDefaultErrorPageBody_(status);
    (void)out_response.setHeader("Content-Type", "text/html");
    (void)out_response.setExpectedContentLength(
        static_cast<unsigned long>(body.size()));

    out.body_source.reset(new StringBodySource(body));
    out.should_close_connection = false;
    return out;
}

}  // namespace server
