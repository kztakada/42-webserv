#ifndef HTTP_STATUS_TYPES_HPP
#define HTTP_STATUS_TYPES_HPP

#include <cstdlib>
#include <map>
#include <sstream>
#include <string>

namespace http
{

// IANA HTTP Status Code Registry準拠
// RFC9110, Section 16.2.1: 定義されたHTTPステータスコードを表すクラス
class HttpStatus
{
   public:
    // 1. Enum定義（クラス内部に隠蔽）
    enum Code
    {
        OK = 200,
        CREATED = 201,
        ACCEPTED = 202,
        NO_CONTENT = 204,
        MULTIPLE_CHOICES = 300,
        MOVED_PERMANENTLY = 301,
        FOUND = 302,
        SEE_OTHER = 303,
        NOT_MODIFIED = 304,
        TEMPORARY_REDIRECT = 307,
        BAD_REQUEST = 400,
        UNAUTHORIZED = 401,
        FORBIDDEN = 403,
        NOT_FOUND = 404,
        NOT_ALLOWED = 405,
        NOT_ACCEPTABLE = 406,
        REQUEST_TIMEOUT = 408,
        PAYLOAD_TOO_LARGE = 413,
        URI_TOO_LONG = 414,
        SERVER_ERROR = 500,
        NOT_IMPLEMENTED = 501,
        SERVICE_UNAVAILABLE = 503,
        HTTP_VERSION_NOT_SUPPORTED = 505,
        UNKNOWN = 0
    };

    // 2. コンストラクタ
    // デフォルトはOK、またはEnumからの暗黙変換を許可
    HttpStatus(Code c = OK) : code_(c) {}

    // 数値からの構築
    explicit HttpStatus(unsigned long status_code)
    {
        code_ = codeFromInt(status_code);
    }

    // 3. std::set のために必須 (オブジェクト同士の比較)
    bool operator<(const HttpStatus& other) const
    {
        return code_ < other.code_;
    }

    // 3. 一般的なロジックのために推奨 (オブジェクト同士の比較)
    bool operator==(const HttpStatus& other) const
    {
        return code_ == other.code_;
    }
    bool operator!=(const HttpStatus& other) const
    {
        return code_ != other.code_;
    }

    // 3. 曖昧さ回避のために推奨 (Enumとの直接比較)
    bool operator==(Code c) const { return code_ == c; }
    bool operator!=(Code c) const { return code_ != c; }

    // 数値との比較
    bool operator==(unsigned long status_code) const
    {
        return code_ == codeFromInt(status_code);
    }
    bool operator!=(unsigned long status_code) const
    {
        return code_ != codeFromInt(status_code);
    }

    // 4. Switch文で使えるようにするためのキャスト演算子
    operator Code() const { return code_; }

    // 5. 数値への変換
    unsigned long toInt() const { return static_cast<unsigned long>(code_); }

    // 6. ステータスコードの取得（数値）
    unsigned long getCode() const { return toInt(); }

    // 7. メッセージの取得
    const char* getMessage() const
    {
        switch (code_)
        {
            case OK:
                return "OK";
            case CREATED:
                return "Created";
            case ACCEPTED:
                return "Accepted";
            case NO_CONTENT:
                return "No Content";
            case MULTIPLE_CHOICES:
                return "Multiple Choices";
            case MOVED_PERMANENTLY:
                return "Moved Permanently";
            case FOUND:
                return "Found";
            case SEE_OTHER:
                return "See Other";
            case NOT_MODIFIED:
                return "Not Modified";
            case TEMPORARY_REDIRECT:
                return "Temporary Redirect";
            case BAD_REQUEST:
                return "Bad Request";
            case UNAUTHORIZED:
                return "Unauthorized";
            case FORBIDDEN:
                return "Forbidden";
            case NOT_FOUND:
                return "Not Found";
            case NOT_ALLOWED:
                return "Method Not Allowed";
            case NOT_ACCEPTABLE:
                return "Not Acceptable";
            case REQUEST_TIMEOUT:
                return "Request Timeout";
            case PAYLOAD_TOO_LARGE:
                return "Payload Too Large";
            case URI_TOO_LONG:
                return "URI Too Long";
            case SERVER_ERROR:
                return "Internal Server Error";
            case NOT_IMPLEMENTED:
                return "Not Implemented";
            case SERVICE_UNAVAILABLE:
                return "Service Unavailable";
            case HTTP_VERSION_NOT_SUPPORTED:
                return "HTTP Version Not Supported";
            default:
                return "Unknown";
        }
    }

    // 8. 文字列からの生成 (fromString)
    static HttpStatus fromString(const char* str)
    {
        if (str == NULL)
            return UNKNOWN;

        // strtoulを使って数値に変換
        char* end;
        unsigned long code = std::strtoul(str, &end, 10);

        // 変換が成功したか確認
        if (end == str || *end != '\0')
            return UNKNOWN;

        return HttpStatus(code);
    }

    static HttpStatus fromString(const std::string& str)
    {
        return fromString(str.c_str());
    }

    // 9. 検証
    static bool isValid(unsigned long status_code)
    {
        return codeFromInt(status_code) != UNKNOWN;
    }

    static bool isValid(const char* str) { return fromString(str) != UNKNOWN; }

    static bool isValid(const std::string& str) { return isValid(str.c_str()); }

    // 10. カテゴリ判定
    bool isInformational() const
    {
        unsigned long c = toInt();
        return c >= 100 && c < 200;
    }

    bool isSuccess() const
    {
        unsigned long c = toInt();
        return c >= 200 && c < 300;
    }

    bool isRedirection() const
    {
        unsigned long c = toInt();
        return c >= 300 && c < 400;
    }

    bool isClientError() const
    {
        unsigned long c = toInt();
        return c >= 400 && c < 500;
    }

    bool isServerError() const
    {
        unsigned long c = toInt();
        return c >= 500 && c < 600;
    }

    bool isError() const { return isClientError() || isServerError(); }

   private:
    Code code_;

    // 数値からCodeへの変換ヘルパー
    static Code codeFromInt(unsigned long status_code)
    {
        switch (status_code)
        {
            case 200:
                return OK;
            case 201:
                return CREATED;
            case 202:
                return ACCEPTED;
            case 204:
                return NO_CONTENT;
            case 300:
                return MULTIPLE_CHOICES;
            case 301:
                return MOVED_PERMANENTLY;
            case 302:
                return FOUND;
            case 303:
                return SEE_OTHER;
            case 304:
                return NOT_MODIFIED;
            case 307:
                return TEMPORARY_REDIRECT;
            case 400:
                return BAD_REQUEST;
            case 401:
                return UNAUTHORIZED;
            case 403:
                return FORBIDDEN;
            case 404:
                return NOT_FOUND;
            case 405:
                return NOT_ALLOWED;
            case 406:
                return NOT_ACCEPTABLE;
            case 408:
                return REQUEST_TIMEOUT;
            case 413:
                return PAYLOAD_TOO_LARGE;
            case 414:
                return URI_TOO_LONG;
            case 500:
                return SERVER_ERROR;
            case 501:
                return NOT_IMPLEMENTED;
            case 503:
                return SERVICE_UNAVAILABLE;
            case 505:
                return HTTP_VERSION_NOT_SUPPORTED;
            default:
                // 標準的なステータスコードの範囲内であればUNKNOWNではなくそのまま保持
                if (status_code >= 100 && status_code < 600)
                    return static_cast<Code>(status_code);
                return UNKNOWN;
        }
    }
};

}  // namespace http

#endif