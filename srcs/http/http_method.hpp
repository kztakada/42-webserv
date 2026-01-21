#ifndef HTTP_HTTP_METHOD_HPP_
#define HTTP_HTTP_METHOD_HPP_

#include <cstring>
#include <string>

namespace http
{

// RFC 7231 Section 4で定義されたHTTPメソッドを表すクラス
class HttpMethod
{
   public:
    // 1. Enum定義（クラス内部に隠蔽）
    enum Type
    {
        GET,
        POST,
        PUT,
        DELETE,
        HEAD,
        OPTIONS,
        PATCH,
        CONNECT,
        TRACE,
        UNKNOWN
    };

    // 2. コンストラクタ
    // デフォルトはUNKNOWN、またはEnumからの暗黙変換を許可
    HttpMethod(Type v = UNKNOWN) : type_(v) {}

    // ---------------------------------------------------------
    // 3. std::set のために必須 (オブジェクト同士の比較)
    // ---------------------------------------------------------
    bool operator<(const HttpMethod& other) const
    {
        return type_ < other.type_;
    }
    // ---------------------------------------------------------
    // 3. 一般的なロジックのために推奨 (オブジェクト同士の比較)
    // ---------------------------------------------------------
    bool operator==(const HttpMethod& other) const
    {
        return type_ == other.type_;
    }
    bool operator!=(const HttpMethod& other) const
    {
        return type_ != other.type_;
    }
    // ---------------------------------------------------------
    // 3. 曖昧さ回避のために推奨 (Enumとの直接比較)
    // これがあれば if (method == HttpMethod::GET) が高速かつ安全に動く
    // ---------------------------------------------------------
    bool operator==(Type t) const { return type_ == t; }

    bool operator!=(Type t) const { return type_ != t; }

    // 4. Switch文で使えるようにするためのキャスト演算子
    // これにより switch(httpMethodInstance) { case HttpMethod::GET: ... }
    // が可能
    operator Type() const { return type_; }

    // 5. 文字列化 (toString)
    // メモリ割り当てを避けるため、基本は const char* を返す
    const char* c_str() const
    {
        switch (type_)
        {
            case GET:
                return "GET";
            case POST:
                return "POST";
            case PUT:
                return "PUT";
            case DELETE:
                return "DELETE";
            case HEAD:
                return "HEAD";
            case OPTIONS:
                return "OPTIONS";
            case PATCH:
                return "PATCH";
            case CONNECT:
                return "CONNECT";
            case TRACE:
                return "TRACE";
            default:
                return "UNKNOWN";
        }
    }

    // std::string が必要な場合のためのラッパー
    std::string toString() const { return std::string(c_str()); }

    // 6. 文字列からの生成 (fromString)
    // ファクトリーメソッドとして static 定義
    static HttpMethod fromString(const char* str)
    {
        if (str == NULL)
            return UNKNOWN;

        // C++98ではstrcmpが最も高速かつ確実
        if (std::strcmp(str, "GET") == 0)
            return GET;
        if (std::strcmp(str, "POST") == 0)
            return POST;
        if (std::strcmp(str, "PUT") == 0)
            return PUT;
        if (std::strcmp(str, "DELETE") == 0)
            return DELETE;
        if (std::strcmp(str, "HEAD") == 0)
            return HEAD;
        if (std::strcmp(str, "OPTIONS") == 0)
            return OPTIONS;
        if (std::strcmp(str, "PATCH") == 0)
            return PATCH;
        if (std::strcmp(str, "CONNECT") == 0)
            return CONNECT;
        if (std::strcmp(str, "TRACE") == 0)
            return TRACE;

        return UNKNOWN;
    }

    // std::string版のオーバーロード
    static HttpMethod fromString(const std::string& str)
    {
        return fromString(str.c_str());
    }

    static bool isValid(const char* str) { return fromString(str) != UNKNOWN; }
    static bool isValid(const std::string& method_str)
    {
        return isValid(method_str.c_str());
    }

   private:
    Type type_;
};

}  // namespace http

#endif