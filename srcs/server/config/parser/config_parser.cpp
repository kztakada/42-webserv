#include "server/config/parser/config_parser.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

#include "http/http_method.hpp"
#include "http/status.hpp"
#include "utils/result.hpp"

namespace server
{

using utils::result::ERROR;
using utils::result::Result;

namespace
{

static bool isSpace_(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f';
}

static bool isTokenDelimiter_(char c)
{
    return isSpace_(c) || c == '{' || c == '}' || c == ';' || c == '#';
}

static Result<unsigned long> parseUnsignedLong_(const std::string& token)
{
    if (token.empty())
    {
        return Result<unsigned long>(ERROR, "number is empty");
    }
    for (size_t i = 0; i < token.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(token[i])))
        {
            return Result<unsigned long>(ERROR, "number is invalid");
        }
    }
    std::stringstream ss(token);
    unsigned long out = 0;
    ss >> out;
    if (ss.fail())
    {
        return Result<unsigned long>(ERROR, "number is invalid");
    }
    return out;
}

static Result<unsigned long> parseSizeBytes_(const std::string& token)
{
    if (token.empty())
    {
        return Result<unsigned long>(ERROR, "client_max_body_size is empty");
    }
    std::string num = token;
    bool is_megabytes = false;
    if (token.size() >= 2)
    {
        const char last = token[token.size() - 1];
        if (last == 'M' || last == 'm')
        {
            is_megabytes = true;
            num = token.substr(0, token.size() - 1);
        }
    }
    Result<unsigned long> parsed = parseUnsignedLong_(num);
    if (parsed.isError())
    {
        return Result<unsigned long>(ERROR, "client_max_body_size is invalid");
    }
    unsigned long bytes = parsed.unwrap();
    if (is_megabytes)
    {
        bytes *= (1024ul * 1024ul);
    }
    return bytes;
}

static bool shouldCheckDuplicateInLocation_(const std::string& directive)
{
    return directive == "client_max_body_size" || directive == "root" ||
           directive == "autoindex" || directive == "upload_store" ||
           directive == "return";
}

static Result<void> checkUniqueServerNamesPerPort_(
    const ServerConfig& config, const VirtualServerConf& new_server)
{
    // 同一 port に対して同一 server_name が複数定義されたらエラー。
    // ただし空文字 server_name は複数許容（最初がデフォルト）
    for (size_t i = 0; i < config.servers.size(); ++i)
    {
        const VirtualServerConf& existing = config.servers[i];
        for (size_t e = 0; e < existing.listens.size(); ++e)
        {
            const std::string existing_port =
                existing.listens[e].port.toString();
            for (size_t n = 0; n < new_server.listens.size(); ++n)
            {
                const std::string new_port =
                    new_server.listens[n].port.toString();
                if (existing_port != new_port)
                {
                    continue;
                }
                for (std::set<std::string>::const_iterator it =
                         new_server.server_names.begin();
                    it != new_server.server_names.end(); ++it)
                {
                    if (it->empty())
                    {
                        continue;
                    }
                    if (existing.server_names.find(*it) !=
                        existing.server_names.end())
                    {
                        return Result<void>(ERROR,
                            "duplicate server_name on same listen port: " +
                                *it);
                    }
                }
            }
        }
    }
    return Result<void>();
}

static Result<http::HttpMethod> parseMethod_(const std::string& token)
{
    if (token == "GET")
    {
        return http::HttpMethod(http::HttpMethod::GET);
    }
    if (token == "POST")
    {
        return http::HttpMethod(http::HttpMethod::POST);
    }
    if (token == "DELETE")
    {
        return http::HttpMethod(http::HttpMethod::DELETE);
    }
    return Result<http::HttpMethod>(ERROR, "invalid http method: " + token);
}

static Result<http::HttpStatus> parseStatus_(const std::string& token)
{
    Result<unsigned long> parsed = parseUnsignedLong_(token);
    if (parsed.isError())
    {
        return Result<http::HttpStatus>(ERROR, "status code is invalid");
    }
    http::HttpStatus st(parsed.unwrap());
    if (st == http::HttpStatus::UNKNOWN)
    {
        return Result<http::HttpStatus>(ERROR, "status code is invalid");
    }
    return st;
}

}  // namespace

Result<char> ConfigParser::ParseContext::getC()
{
    if (buf_idx_ >= file_content_.size())
    {
        return Result<char>(ERROR, "unexpected EOF");
    }
    return file_content_[buf_idx_++];
}

char ConfigParser::ParseContext::ungetC()
{
    if (buf_idx_ == 0)
    {
        return '\0';
    }
    --buf_idx_;
    return file_content_[buf_idx_];
}

void ConfigParser::ParseContext::skipSpaces()
{
    while (buf_idx_ < file_content_.size())
    {
        const char c = file_content_[buf_idx_];
        if (isSpace_(c))
        {
            ++buf_idx_;
            continue;
        }
        if (c == '#')
        {
            // コメント: 行末まで読み飛ばす
            while (buf_idx_ < file_content_.size() &&
                   file_content_[buf_idx_] != '\n')
            {
                ++buf_idx_;
            }
            continue;
        }
        break;
    }
}

Result<bool> ConfigParser::ParseContext::getCAndExpectSemicolon()
{
    skipSpaces();
    Result<char> c = getC();
    if (c.isError())
    {
        return Result<bool>(ERROR, c.getErrorMessage());
    }
    if (c.unwrap() != ';')
    {
        return Result<bool>(ERROR, "expected ';'");
    }
    return true;
}

Result<std::string> ConfigParser::ParseContext::getWord()
{
    skipSpaces();
    if (isEofReached())
    {
        return Result<std::string>(ERROR, std::string(), "unexpected EOF");
    }

    const char c = file_content_[buf_idx_];
    if (c == '{' || c == '}' || c == ';')
    {
        ++buf_idx_;
        return std::string(1, c);
    }

    std::string out;
    while (buf_idx_ < file_content_.size())
    {
        const char cur = file_content_[buf_idx_];
        if (isTokenDelimiter_(cur) || cur == '{' || cur == '}' || cur == ';')
        {
            break;
        }
        out.push_back(cur);
        ++buf_idx_;
    }
    if (out.empty())
    {
        return Result<std::string>(ERROR, std::string(), "unexpected token");
    }
    return out;
}

bool ConfigParser::ParseContext::isEofReached() const
{
    return buf_idx_ >= file_content_.size();
}

bool ConfigParser::ParseContext::isDirectiveSetInLocation(
    const std::string& directive) const
{
    return location_set_directives_.find(directive) !=
           location_set_directives_.end();
}

void ConfigParser::ParseContext::clearLocationSetDirectives()
{
    location_set_directives_.clear();
}

void ConfigParser::ParseContext::markDirectiveSetInLocation(
    const std::string& directive)
{
    location_set_directives_.insert(directive);
}

Result<ServerConfig> ConfigParser::parseFile(const std::string& filepath)
{
    std::ifstream ifs(filepath.c_str());
    if (!ifs)
    {
        return Result<ServerConfig>(ERROR, "failed to open config file");
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    return parseConfigInternal(ss.str());
}

Result<ServerConfig> ConfigParser::parseData(const std::string& data)
{
    return parseConfigInternal(data);
}

Result<ServerConfig> ConfigParser::parseConfigInternal(const std::string& data)
{
    ServerConfig config;
    size_t idx = 0;
    std::set<std::string> location_set_directives;
    ParseContext ctx(data, idx, location_set_directives);

    ctx.skipSpaces();
    if (ctx.isEofReached())
    {
        return Result<ServerConfig>(ERROR, "config is empty");
    }

    while (!ctx.isEofReached())
    {
        ctx.skipSpaces();
        if (ctx.isEofReached())
        {
            break;
        }
        Result<std::string> w = ctx.getWord();
        if (w.isError())
        {
            return Result<ServerConfig>(ERROR, w.getErrorMessage());
        }
        if (w.unwrap() == "server")
        {
            Result<void> r = parseServerBlock(ctx, config);
            if (r.isError())
            {
                return Result<ServerConfig>(ERROR, r.getErrorMessage());
            }
            continue;
        }
        return Result<ServerConfig>(ERROR, "unexpected token: " + w.unwrap());
    }

    if (!config.isValid())
    {
        return Result<ServerConfig>(ERROR, "config is invalid");
    }
    return config;
}

Result<void> ConfigParser::parseServerBlock(
    ParseContext& ctx, ServerConfig& config)
{
    VirtualServerConfMaker vserver;

    Result<std::string> open = ctx.getWord();
    if (open.isError())
    {
        return Result<void>(ERROR, open.getErrorMessage());
    }
    if (open.unwrap() != "{")
    {
        return Result<void>(
            ERROR, "expected '{' but got '" + open.unwrap() + "'");
    }

    while (true)
    {
        Result<std::string> word = ctx.getWord();
        if (word.isError())
        {
            return Result<void>(ERROR, word.getErrorMessage());
        }
        if (word.unwrap() == "}")
        {
            break;
        }
        if (word.unwrap() == "listen")
        {
            Result<void> r = parseListenDirective(ctx, vserver);
            if (r.isError())
            {
                return r;
            }
            continue;
        }
        if (word.unwrap() == "server_name")
        {
            Result<void> r = parseServerNameDirective(ctx, vserver);
            if (r.isError())
            {
                return r;
            }
            continue;
        }
        if (word.unwrap() == "root")
        {
            Result<std::string> path = ctx.getWord();
            if (path.isError())
            {
                return Result<void>(ERROR, path.getErrorMessage());
            }
            Result<void> set_r = vserver.setRootDir(path.unwrap());
            if (set_r.isError())
            {
                return set_r;
            }
            Result<std::string> semi = ctx.getWord();
            if (semi.isError())
            {
                return Result<void>(ERROR, semi.getErrorMessage());
            }
            if (semi.unwrap() != ";")
            {
                return Result<void>(ERROR, "expected ';'");
            }
            continue;
        }
        if (word.unwrap() == "index")
        {
            bool has_any = false;
            while (true)
            {
                Result<std::string> tok = ctx.getWord();
                if (tok.isError())
                {
                    return Result<void>(ERROR, tok.getErrorMessage());
                }
                if (tok.unwrap() == ";")
                {
                    break;
                }
                has_any = true;
                Result<void> r = vserver.appendIndexPage(tok.unwrap());
                if (r.isError())
                {
                    return r;
                }
            }
            if (!has_any)
            {
                return Result<void>(
                    ERROR, "index requires at least one argument");
            }
            continue;
        }
        if (word.unwrap() == "client_max_body_size")
        {
            Result<std::string> tok = ctx.getWord();
            if (tok.isError())
            {
                return Result<void>(ERROR, tok.getErrorMessage());
            }
            Result<unsigned long> size = parseSizeBytes_(tok.unwrap());
            if (size.isError())
            {
                return Result<void>(ERROR, size.getErrorMessage());
            }
            Result<void> r = vserver.setClientMaxBodySize(size.unwrap());
            if (r.isError())
            {
                return r;
            }
            Result<std::string> semi = ctx.getWord();
            if (semi.isError())
            {
                return Result<void>(ERROR, semi.getErrorMessage());
            }
            if (semi.unwrap() != ";")
            {
                return Result<void>(ERROR, "expected ';'");
            }
            continue;
        }
        if (word.unwrap() == "error_page")
        {
            Result<std::string> code_tok = ctx.getWord();
            Result<std::string> path_tok = ctx.getWord();
            if (code_tok.isError() || path_tok.isError())
            {
                return Result<void>(ERROR, "error_page requires code and path");
            }
            Result<http::HttpStatus> st = parseStatus_(code_tok.unwrap());
            if (st.isError())
            {
                return Result<void>(ERROR, st.getErrorMessage());
            }
            Result<void> r =
                vserver.appendErrorPage(st.unwrap(), path_tok.unwrap());
            if (r.isError())
            {
                return r;
            }
            Result<std::string> semi = ctx.getWord();
            if (semi.isError())
            {
                return Result<void>(ERROR, semi.getErrorMessage());
            }
            if (semi.unwrap() != ";")
            {
                return Result<void>(ERROR, "expected ';'");
            }
            continue;
        }
        if (word.unwrap() == "location")
        {
            LocationDirectiveConfMaker location;
            Result<std::string> maybe_back_or_path = ctx.getWord();
            if (maybe_back_or_path.isError())
            {
                return Result<void>(
                    ERROR, maybe_back_or_path.getErrorMessage());
            }
            bool is_back = false;
            std::string path_pattern = maybe_back_or_path.unwrap();
            if (path_pattern == "back")
            {
                is_back = true;
                Result<std::string> p = ctx.getWord();
                if (p.isError())
                {
                    return Result<void>(ERROR, p.getErrorMessage());
                }
                path_pattern = p.unwrap();
            }
            Result<void> pr = location.setPathPattern(path_pattern);
            if (pr.isError())
            {
                return pr;
            }
            Result<void> lr = parseLocationBlock(ctx, location, is_back);
            if (lr.isError())
            {
                return lr;
            }
            Result<void> ar = vserver.appendLocation(location.build());
            if (ar.isError())
            {
                return ar;
            }
            continue;
        }

        return Result<void>(
            ERROR, "unknown directive in server: " + word.unwrap());
    }

    VirtualServerConf built = vserver.build();
    if (!built.isValid())
    {
        return Result<void>(ERROR, "server block is invalid");
    }

    Result<void> uniq = checkUniqueServerNamesPerPort_(config, built);
    if (uniq.isError())
    {
        return uniq;
    }

    Result<void> append = config.appendServer(built);
    if (append.isError())
    {
        return append;
    }
    return Result<void>();
}

Result<void> ConfigParser::parseListenDirective(
    ParseContext& ctx, VirtualServerConfMaker& vserver)
{
    Result<std::string> word = ctx.getWord();
    if (word.isError())
    {
        return Result<void>(ERROR, word.getErrorMessage());
    }
    if (!isValidListenArgument(word.unwrap()))
    {
        return Result<void>(ERROR, "listen argument is invalid");
    }
    std::pair<std::string, std::string> ip_port =
        splitToIpAndPort(word.unwrap());
    Result<void> r = vserver.appendListen(ip_port.first, ip_port.second);
    if (r.isError())
    {
        return r;
    }
    Result<std::string> semi = ctx.getWord();
    if (semi.isError())
    {
        return Result<void>(ERROR, semi.getErrorMessage());
    }
    if (semi.unwrap() != ";")
    {
        return Result<void>(ERROR, "expected ';'");
    }
    return Result<void>();
}

Result<void> ConfigParser::parseServerNameDirective(
    ParseContext& ctx, VirtualServerConfMaker& vserver)
{
    bool has_any = false;
    while (true)
    {
        Result<std::string> w = ctx.getWord();
        if (w.isError())
        {
            return Result<void>(ERROR, w.getErrorMessage());
        }
        if (w.unwrap() == ";")
        {
            break;
        }
        has_any = true;
        if (!isDomainName(w.unwrap()))
        {
            return Result<void>(ERROR, "server_name is invalid: " + w.unwrap());
        }
        Result<void> r = vserver.appendServerName(w.unwrap());
        if (r.isError())
        {
            return r;
        }
    }
    if (!has_any)
    {
        // server_name が省略された場合は空扱い、でも directive
        // があるのに引数なしはエラー
        return Result<void>(
            ERROR, "server_name requires at least one argument");
    }
    return Result<void>();
}

Result<void> ConfigParser::parseLocationBlock(ParseContext& ctx,
    LocationDirectiveConfMaker& location, bool is_location_back)
{
    if (is_location_back)
    {
        Result<void> r = location.setIsBackwardSearch(true);
        if (r.isError())
        {
            return r;
        }
    }

    // '{'
    Result<std::string> open = ctx.getWord();
    if (open.isError())
    {
        return Result<void>(ERROR, open.getErrorMessage());
    }
    if (open.unwrap() != "{")
    {
        return Result<void>(
            ERROR, "expected '{' but got '" + open.unwrap() + "'");
    }

    // location 内の重複ディレクティブ検出用
    ctx.clearLocationSetDirectives();

    while (true)
    {
        Result<std::string> directive = ctx.getWord();
        if (directive.isError())
        {
            return Result<void>(ERROR, directive.getErrorMessage());
        }
        if (directive.unwrap() == "}")
        {
            break;
        }

        if (shouldCheckDuplicateInLocation_(directive.unwrap()) &&
            ctx.isDirectiveSetInLocation(directive.unwrap()))
        {
            return Result<void>(ERROR,
                "directive is duplicate in location: " + directive.unwrap());
        }
        if (shouldCheckDuplicateInLocation_(directive.unwrap()))
        {
            ctx.markDirectiveSetInLocation(directive.unwrap());
        }

        if (directive.unwrap() == "allow_methods")
        {
            Result<void> r = parseAllowMethodDirective(ctx, location);
            if (r.isError())
            {
                return r;
            }
            continue;
        }
        if (directive.unwrap() == "cgi_extension")
        {
            Result<void> r = parseCgiExecutorDirective(ctx, location);
            if (r.isError())
            {
                return r;
            }
            continue;
        }
        if (directive.unwrap() == "return")
        {
            Result<void> r = parseReturnDirective(ctx, location);
            if (r.isError())
            {
                return r;
            }
            continue;
        }
        if (directive.unwrap() == "root")
        {
            Result<std::string> path = ctx.getWord();
            if (path.isError())
            {
                return Result<void>(ERROR, path.getErrorMessage());
            }
            Result<void> r = location.setRootDir(path.unwrap());
            if (r.isError())
            {
                return r;
            }
            Result<std::string> semi = ctx.getWord();
            if (semi.isError())
            {
                return Result<void>(ERROR, semi.getErrorMessage());
            }
            if (semi.unwrap() != ";")
            {
                return Result<void>(ERROR, "expected ';'");
            }
            continue;
        }
        if (directive.unwrap() == "index")
        {
            bool has_any = false;
            while (true)
            {
                Result<std::string> tok = ctx.getWord();
                if (tok.isError())
                {
                    return Result<void>(ERROR, tok.getErrorMessage());
                }
                if (tok.unwrap() == ";")
                {
                    break;
                }
                has_any = true;
                Result<void> r = location.appendIndexPage(tok.unwrap());
                if (r.isError())
                {
                    return r;
                }
            }
            if (!has_any)
            {
                return Result<void>(
                    ERROR, "index requires at least one argument");
            }
            continue;
        }
        if (directive.unwrap() == "client_max_body_size")
        {
            Result<std::string> tok = ctx.getWord();
            if (tok.isError())
            {
                return Result<void>(ERROR, tok.getErrorMessage());
            }
            Result<unsigned long> size = parseSizeBytes_(tok.unwrap());
            if (size.isError())
            {
                return Result<void>(ERROR, size.getErrorMessage());
            }
            Result<void> r = location.setClientMaxBodySize(size.unwrap());
            if (r.isError())
            {
                return r;
            }
            Result<std::string> semi = ctx.getWord();
            if (semi.isError())
            {
                return Result<void>(ERROR, semi.getErrorMessage());
            }
            if (semi.unwrap() != ";")
            {
                return Result<void>(ERROR, "expected ';'");
            }
            continue;
        }
        if (directive.unwrap() == "error_page")
        {
            Result<std::string> code_tok = ctx.getWord();
            Result<std::string> path_tok = ctx.getWord();
            if (code_tok.isError() || path_tok.isError())
            {
                return Result<void>(ERROR, "error_page requires code and path");
            }
            Result<http::HttpStatus> st = parseStatus_(code_tok.unwrap());
            if (st.isError())
            {
                return Result<void>(ERROR, st.getErrorMessage());
            }
            Result<void> r =
                location.appendErrorPage(st.unwrap(), path_tok.unwrap());
            if (r.isError())
            {
                return r;
            }
            Result<std::string> semi = ctx.getWord();
            if (semi.isError())
            {
                return Result<void>(ERROR, semi.getErrorMessage());
            }
            if (semi.unwrap() != ";")
            {
                return Result<void>(ERROR, "expected ';'");
            }
            continue;
        }
        if (directive.unwrap() == "autoindex")
        {
            Result<std::string> tok = ctx.getWord();
            if (tok.isError())
            {
                return Result<void>(ERROR, tok.getErrorMessage());
            }
            Result<bool> on = parseOnOff(tok.unwrap());
            if (on.isError())
            {
                return Result<void>(ERROR, on.getErrorMessage());
            }
            Result<void> r = location.setAutoIndex(on.unwrap());
            if (r.isError())
            {
                return r;
            }
            Result<std::string> semi = ctx.getWord();
            if (semi.isError())
            {
                return Result<void>(ERROR, semi.getErrorMessage());
            }
            if (semi.unwrap() != ";")
            {
                return Result<void>(ERROR, "expected ';'");
            }
            continue;
        }
        if (directive.unwrap() == "upload_store")
        {
            Result<std::string> tok = ctx.getWord();
            if (tok.isError())
            {
                return Result<void>(ERROR, tok.getErrorMessage());
            }
            Result<void> r = location.setUploadStore(tok.unwrap());
            if (r.isError())
            {
                return r;
            }
            Result<std::string> semi = ctx.getWord();
            if (semi.isError())
            {
                return Result<void>(ERROR, semi.getErrorMessage());
            }
            if (semi.unwrap() != ";")
            {
                return Result<void>(ERROR, "expected ';'");
            }
            continue;
        }

        return Result<void>(
            ERROR, "unknown directive in location: " + directive.unwrap());
    }

    return Result<void>();
}

Result<void> ConfigParser::parseAllowMethodDirective(
    ParseContext& ctx, LocationDirectiveConfMaker& location)
{
    bool has_any = false;
    while (true)
    {
        Result<std::string> tok = ctx.getWord();
        if (tok.isError())
        {
            return Result<void>(ERROR, tok.getErrorMessage());
        }
        if (tok.unwrap() == ";")
        {
            break;
        }
        has_any = true;
        Result<http::HttpMethod> m = parseMethod_(tok.unwrap());
        if (m.isError())
        {
            return Result<void>(ERROR, m.getErrorMessage());
        }
        Result<void> r = location.appendAllowedMethod(m.unwrap());
        if (r.isError())
        {
            return r;
        }
    }
    if (!has_any)
    {
        return Result<void>(
            ERROR, "allow_methods requires at least one method");
    }
    return Result<void>();
}

Result<void> ConfigParser::parseCgiExecutorDirective(
    ParseContext& ctx, LocationDirectiveConfMaker& location)
{
    Result<std::string> ext = ctx.getWord();
    Result<std::string> path = ctx.getWord();
    if (ext.isError() || path.isError())
    {
        return Result<void>(ERROR, "cgi_extension requires ext and path");
    }
    Result<void> r = location.appendCgiExtension(ext.unwrap(), path.unwrap());
    if (r.isError())
    {
        return r;
    }
    Result<std::string> semi = ctx.getWord();
    if (semi.isError())
    {
        return Result<void>(ERROR, semi.getErrorMessage());
    }
    if (semi.unwrap() != ";")
    {
        return Result<void>(ERROR, "expected ';'");
    }
    return Result<void>();
}

Result<void> ConfigParser::parseReturnDirective(
    ParseContext& ctx, LocationDirectiveConfMaker& location)
{
    Result<std::string> code_tok = ctx.getWord();
    Result<std::string> url_tok = ctx.getWord();
    if (code_tok.isError() || url_tok.isError())
    {
        return Result<void>(ERROR, "return requires code and url");
    }
    Result<http::HttpStatus> st = parseStatus_(code_tok.unwrap());
    if (st.isError())
    {
        return Result<void>(ERROR, st.getErrorMessage());
    }
    Result<void> r = location.setRedirect(st.unwrap(), url_tok.unwrap());
    if (r.isError())
    {
        return r;
    }
    Result<std::string> semi = ctx.getWord();
    if (semi.isError())
    {
        return Result<void>(ERROR, semi.getErrorMessage());
    }
    if (semi.unwrap() != ";")
    {
        return Result<void>(ERROR, "expected ';'");
    }
    return Result<void>();
}

bool ConfigParser::isDigits(const std::string& str)
{
    if (str.empty())
    {
        return false;
    }
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(str[i])))
        {
            return false;
        }
    }
    return true;
}

bool ConfigParser::isDomainLabel(const std::string& label)
{
    if (label.empty() || label.size() > kMaxDomainLabelLength)
    {
        return false;
    }
    if (label[0] == '-' || label[label.size() - 1] == '-')
    {
        return false;
    }
    for (size_t i = 0; i < label.size(); ++i)
    {
        const char c = label[i];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-')
        {
            continue;
        }
        return false;
    }
    return true;
}

bool ConfigParser::isDomainName(std::string domain_name)
{
    if (domain_name.empty() || domain_name.size() > kMaxDomainLength)
    {
        return false;
    }

    // optional :port
    std::string port;
    std::string host = domain_name;
    const size_t colon = domain_name.rfind(':');
    if (colon != std::string::npos)
    {
        host = domain_name.substr(0, colon);
        port = domain_name.substr(colon + 1);
        if (host.empty())
        {
            return false;
        }
        if (!isValidPort(port))
        {
            return false;
        }
    }

    // split by '.'
    size_t start = 0;
    while (start < host.size())
    {
        size_t dot = host.find('.', start);
        std::string label;
        if (dot == std::string::npos)
        {
            label = host.substr(start);
            start = host.size();
        }
        else
        {
            label = host.substr(start, dot - start);
            start = dot + 1;
        }
        if (!isDomainLabel(label))
        {
            return false;
        }
    }
    return true;
}

bool ConfigParser::isHttpMethod(const std::string& method)
{
    return method == "GET" || method == "POST" || method == "DELETE";
}

bool ConfigParser::isValidHttpStatusCode(const std::string& code)
{
    Result<unsigned long> parsed = parseUnsignedLong_(code);
    if (parsed.isError())
    {
        return false;
    }
    const unsigned long v = parsed.unwrap();
    return v >= 100 && v < 600;
}

bool ConfigParser::isValidListenArgument(const std::string& word)
{
    if (word.empty())
    {
        return false;
    }
    std::pair<std::string, std::string> ip_port = splitToIpAndPort(word);
    if (!isValidPort(ip_port.second))
    {
        return false;
    }
    if (!ip_port.first.empty() && !isValidIp(ip_port.first))
    {
        return false;
    }
    return true;
}

bool ConfigParser::isValidIp(const std::string& ip)
{
    if (ip.empty())
    {
        return false;
    }
    int parts = 0;
    size_t start = 0;
    while (start <= ip.size())
    {
        size_t dot = ip.find('.', start);
        std::string part;
        if (dot == std::string::npos)
        {
            part = ip.substr(start);
            start = ip.size() + 1;
        }
        else
        {
            part = ip.substr(start, dot - start);
            start = dot + 1;
        }
        if (part.empty() || part.size() > 3 || !isDigits(part))
        {
            return false;
        }
        std::stringstream ss(part);
        unsigned long n = 0;
        ss >> n;
        if (ss.fail() || n > 255)
        {
            return false;
        }
        ++parts;
        if (dot == std::string::npos)
        {
            break;
        }
    }
    return parts == 4;
}

std::pair<std::string, std::string> ConfigParser::splitToIpAndPort(
    const std::string& ip_port)
{
    const size_t pos = ip_port.find(':');
    if (pos == std::string::npos)
    {
        return std::make_pair(std::string(), ip_port);
    }
    return std::make_pair(ip_port.substr(0, pos), ip_port.substr(pos + 1));
}

bool ConfigParser::isValidPort(const std::string& port)
{
    if (port.empty() || !isDigits(port))
    {
        return false;
    }
    std::stringstream ss(port);
    unsigned long n = 0;
    ss >> n;
    if (ss.fail())
    {
        return false;
    }
    return n <= kMaxPortNumber;
}

Result<bool> ConfigParser::parseOnOff(const std::string& on_or_off)
{
    if (on_or_off == "on")
    {
        return true;
    }
    if (on_or_off == "off")
    {
        return false;
    }
    return Result<bool>(ERROR, "expected 'on' or 'off'");
}

bool ConfigParser::isEofReached(const ParseContext& ctx)
{
    return ctx.isEofReached();
}

bool ConfigParser::isDirectiveSetInLocation(
    const ParseContext& ctx, const std::string& directive)
{
    return ctx.isDirectiveSetInLocation(directive);
}

void ConfigParser::appendConf(const VirtualServerConf& virtual_server_conf)
{
    resultConfig_.appendServer(virtual_server_conf);
}

}  // namespace server
