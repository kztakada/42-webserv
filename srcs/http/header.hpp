#ifndef HTTP_HEADER_TYPES_HPP_
#define HTTP_HEADER_TYPES_HPP_

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// RFC 9110（HTTP Semantics）の セクション 5.1:
// ヘッダーフィールド名は大文字小文字を区別しない
struct CaseInsensitiveCompare
{
    struct CharLess
    {
        bool operator()(unsigned char a, unsigned char b) const
        {
            return std::tolower(static_cast<int>(a)) <
                   std::tolower(static_cast<int>(b));
        }
    };

    bool operator()(const std::string& lhs, const std::string& rhs) const
    {
        return std::lexicographical_compare(
            lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), CharLess());
    }
};

/* Example Usage:

        // RFC準拠の自動マージ
        HeaderMap headers;
        headers["Content-Type"].push_back("text/html");
        headers["content-type"].push_back("charset=utf-8");  //
   同じエントリに追加
        // → headers["Content-Type"] = ["text/html", "charset=utf-8"]

        // 型安全なヘッダー操作
        response.setHeader(HeaderName(HeaderName::CONTENT_TYPE).toString(),
   "application/json");
        response.setHeader(HeaderName(HeaderName::SET_COOKIE).toString(),
   "session=abc");

        // カスタムヘッダーも引き続き使用可能
        response.setHeader("X-Request-ID", uuid);

*/

namespace http
{

// RFC 9110 セクション5.5 ~ セクション5.6:ヘッダーフィールドの型
typedef std::map<std::string, std::vector<std::string>, CaseInsensitiveCompare>
    HeaderMap;

// IANA Hypertext Transfer Protocol (HTTP) Field Name Registry準拠
// RFC 9110で定義された標準ヘッダーを表すクラス
class HeaderName
{
   public:
    // 1. Enum定義（クラス内部に隠蔽）
    // RFC 9110 / 9112:
    // 最も基本的かつ必須のセマンティクス（意味論）を持つヘッダーを選出
    enum Type
    {
        UNKNOWN,
        // Semantics and Content
        CONTENT_TYPE,
        CONTENT_ENCODING,
        CONTENT_LANGUAGE,
        CONTENT_LOCATION,
        // Message Syntax and Routing
        HOST,
        CONNECTION,
        TRANSFER_ENCODING,
        CONTENT_LENGTH,
        TE,
        TRAILER,
        UPGRADE,
        // Request Headers
        ACCEPT,
        ACCEPT_CHARSET,
        ACCEPT_ENCODING,
        ACCEPT_LANGUAGE,
        USER_AGENT,
        REFERER,
        // Response Headers
        ALLOW,
        LOCATION,
        SERVER,
        // Conditional Requests
        ETAG,
        LAST_MODIFIED,
        IF_MATCH,
        IF_NONE_MATCH,
        IF_MODIFIED_SINCE,
        IF_UNMODIFIED_SINCE,
        // Range Requests
        ACCEPT_RANGES,
        RANGE,
        CONTENT_RANGE,
        // Caching
        CACHE_CONTROL,
        EXPIRES,
        PRAGMA,
        AGE,
        // Authentication
        AUTHORIZATION,
        WWW_AUTHENTICATE,
        // HTTP State Management (Cookies)
        COOKIE,
        SET_COOKIE,
        // その他の一般的なヘッダー
        DATE,
        VIA,
        WARNING
    };

    // 2. コンストラクタ
    // デフォルトはUNKNOWN、またはEnumからの暗黙変換を許可
    HeaderName(Type v = UNKNOWN) : type_(v) {}

    // 3. std::set のために必須 (オブジェクト同士の比較)
    bool operator<(const HeaderName& other) const
    {
        return type_ < other.type_;
    }

    // 3. 一般的なロジックのために推奨 (オブジェクト同士の比較)
    bool operator==(const HeaderName& other) const
    {
        return type_ == other.type_;
    }
    bool operator!=(const HeaderName& other) const
    {
        return type_ != other.type_;
    }

    // 3. 曖昧さ回避のために推奨 (Enumとの直接比較)
    bool operator==(Type t) const { return type_ == t; }
    bool operator!=(Type t) const { return type_ != t; }

    // 4. Switch文で使えるようにするためのキャスト演算子
    operator Type() const { return type_; }

    // 5. 文字列化 (toString)
    // 正規化された大文字小文字表記で返す
    const char* c_str() const
    {
        switch (type_)
        {
            case CONTENT_TYPE:
                return "Content-Type";
            case CONTENT_ENCODING:
                return "Content-Encoding";
            case CONTENT_LANGUAGE:
                return "Content-Language";
            case CONTENT_LOCATION:
                return "Content-Location";
            case HOST:
                return "Host";
            case CONNECTION:
                return "Connection";
            case TRANSFER_ENCODING:
                return "Transfer-Encoding";
            case CONTENT_LENGTH:
                return "Content-Length";
            case TE:
                return "TE";
            case TRAILER:
                return "Trailer";
            case UPGRADE:
                return "Upgrade";
            case ACCEPT:
                return "Accept";
            case ACCEPT_CHARSET:
                return "Accept-Charset";
            case ACCEPT_ENCODING:
                return "Accept-Encoding";
            case ACCEPT_LANGUAGE:
                return "Accept-Language";
            case USER_AGENT:
                return "User-Agent";
            case REFERER:
                return "Referer";
            case ALLOW:
                return "Allow";
            case LOCATION:
                return "Location";
            case SERVER:
                return "Server";
            case ETAG:
                return "ETag";
            case LAST_MODIFIED:
                return "Last-Modified";
            case IF_MATCH:
                return "If-Match";
            case IF_NONE_MATCH:
                return "If-None-Match";
            case IF_MODIFIED_SINCE:
                return "If-Modified-Since";
            case IF_UNMODIFIED_SINCE:
                return "If-Unmodified-Since";
            case ACCEPT_RANGES:
                return "Accept-Ranges";
            case RANGE:
                return "Range";
            case CONTENT_RANGE:
                return "Content-Range";
            case CACHE_CONTROL:
                return "Cache-Control";
            case EXPIRES:
                return "Expires";
            case PRAGMA:
                return "Pragma";
            case AGE:
                return "Age";
            case AUTHORIZATION:
                return "Authorization";
            case WWW_AUTHENTICATE:
                return "WWW-Authenticate";
            case COOKIE:
                return "Cookie";
            case SET_COOKIE:
                return "Set-Cookie";
            case DATE:
                return "Date";
            case VIA:
                return "Via";
            case WARNING:
                return "Warning";
            default:
                return "";
        }
    }

    std::string toString() const { return std::string(c_str()); }

    // 6. 文字列からの生成 (fromString)
    // 大文字小文字を区別しない比較
    static HeaderName fromString(const char* name)
    {
        if (name == NULL)
            return UNKNOWN;

        // 大文字小文字を区別しない比較のためのヘルパー
        struct CaseInsensitiveEqual
        {
            static bool equals(const char* a, const char* b)
            {
                while (*a && *b)
                {
                    if (std::tolower(static_cast<unsigned char>(*a)) !=
                        std::tolower(static_cast<unsigned char>(*b)))
                        return false;
                    ++a;
                    ++b;
                }
                return *a == *b;
            }
        };

        if (CaseInsensitiveEqual::equals(name, "Content-Type"))
            return CONTENT_TYPE;
        if (CaseInsensitiveEqual::equals(name, "Content-Encoding"))
            return CONTENT_ENCODING;
        if (CaseInsensitiveEqual::equals(name, "Content-Language"))
            return CONTENT_LANGUAGE;
        if (CaseInsensitiveEqual::equals(name, "Content-Location"))
            return CONTENT_LOCATION;
        if (CaseInsensitiveEqual::equals(name, "Host"))
            return HOST;
        if (CaseInsensitiveEqual::equals(name, "Connection"))
            return CONNECTION;
        if (CaseInsensitiveEqual::equals(name, "Transfer-Encoding"))
            return TRANSFER_ENCODING;
        if (CaseInsensitiveEqual::equals(name, "Content-Length"))
            return CONTENT_LENGTH;
        if (CaseInsensitiveEqual::equals(name, "TE"))
            return TE;
        if (CaseInsensitiveEqual::equals(name, "Trailer"))
            return TRAILER;
        if (CaseInsensitiveEqual::equals(name, "Upgrade"))
            return UPGRADE;
        if (CaseInsensitiveEqual::equals(name, "Accept"))
            return ACCEPT;
        if (CaseInsensitiveEqual::equals(name, "Accept-Charset"))
            return ACCEPT_CHARSET;
        if (CaseInsensitiveEqual::equals(name, "Accept-Encoding"))
            return ACCEPT_ENCODING;
        if (CaseInsensitiveEqual::equals(name, "Accept-Language"))
            return ACCEPT_LANGUAGE;
        if (CaseInsensitiveEqual::equals(name, "User-Agent"))
            return USER_AGENT;
        if (CaseInsensitiveEqual::equals(name, "Referer"))
            return REFERER;
        if (CaseInsensitiveEqual::equals(name, "Allow"))
            return ALLOW;
        if (CaseInsensitiveEqual::equals(name, "Location"))
            return LOCATION;
        if (CaseInsensitiveEqual::equals(name, "Server"))
            return SERVER;
        if (CaseInsensitiveEqual::equals(name, "ETag"))
            return ETAG;
        if (CaseInsensitiveEqual::equals(name, "Last-Modified"))
            return LAST_MODIFIED;
        if (CaseInsensitiveEqual::equals(name, "If-Match"))
            return IF_MATCH;
        if (CaseInsensitiveEqual::equals(name, "If-None-Match"))
            return IF_NONE_MATCH;
        if (CaseInsensitiveEqual::equals(name, "If-Modified-Since"))
            return IF_MODIFIED_SINCE;
        if (CaseInsensitiveEqual::equals(name, "If-Unmodified-Since"))
            return IF_UNMODIFIED_SINCE;
        if (CaseInsensitiveEqual::equals(name, "Accept-Ranges"))
            return ACCEPT_RANGES;
        if (CaseInsensitiveEqual::equals(name, "Range"))
            return RANGE;
        if (CaseInsensitiveEqual::equals(name, "Content-Range"))
            return CONTENT_RANGE;
        if (CaseInsensitiveEqual::equals(name, "Cache-Control"))
            return CACHE_CONTROL;
        if (CaseInsensitiveEqual::equals(name, "Expires"))
            return EXPIRES;
        if (CaseInsensitiveEqual::equals(name, "Pragma"))
            return PRAGMA;
        if (CaseInsensitiveEqual::equals(name, "Age"))
            return AGE;
        if (CaseInsensitiveEqual::equals(name, "Authorization"))
            return AUTHORIZATION;
        if (CaseInsensitiveEqual::equals(name, "WWW-Authenticate"))
            return WWW_AUTHENTICATE;
        if (CaseInsensitiveEqual::equals(name, "Cookie"))
            return COOKIE;
        if (CaseInsensitiveEqual::equals(name, "Set-Cookie"))
            return SET_COOKIE;
        if (CaseInsensitiveEqual::equals(name, "Date"))
            return DATE;
        if (CaseInsensitiveEqual::equals(name, "Via"))
            return VIA;
        if (CaseInsensitiveEqual::equals(name, "Warning"))
            return WARNING;

        return UNKNOWN;
    }

    static HeaderName fromString(const std::string& name)
    {
        return fromString(name.c_str());
    }

    // 7. 検証
    static bool isValid(const char* name)
    {
        if (name == NULL)
            return false;
        if (*name == '\0')
            return false;

        // RFC 9110: field-name = token
        // token は 1 文字以上の tchar の並び
        // tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" /
        //         "-" / "." / "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA
        for (const char* p = name; *p; ++p)
        {
            unsigned char c = static_cast<unsigned char>(*p);
            if (std::isalnum(c))
                continue;
            switch (c)
            {
                case '!':
                case '#':
                case '$':
                case '%':
                case '&':
                case '\'':
                case '*':
                case '+':
                case '-':
                case '.':
                case '^':
                case '_':
                case '`':
                case '|':
                case '~':
                    break;
                default:
                    return false;
            }
        }
        return true;
    }

    static bool isValid(const std::string& name)
    {
        return isValid(name.c_str());
    }

   private:
    Type type_;
};

}  // namespace http

#endif
