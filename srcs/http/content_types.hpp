#ifndef HTTP_CONTENT_TYPES_HPP_
#define HTTP_CONTENT_TYPES_HPP_

#include <cstring>
#include <map>
#include <string>

namespace http
{

// IANA Media Types Registry準拠
// MIMEタイプを表すクラス
class ContentType
{
   public:
    // 1. Enum定義（クラス内部に隠蔽）
    // モダンなWeb開発において、ブラウザとサーバーの間でやり取りされる頻度が最も高い標準的なメディアタイプ
    enum Type
    {
        UNKNOWN,
        // text/*
        TEXT_HTML,
        TEXT_PLAIN,
        TEXT_CSS,
        TEXT_JAVASCRIPT,
        TEXT_XML,
        TEXT_CSV,
        // application/*
        APPLICATION_JSON,
        APPLICATION_XML,
        APPLICATION_PDF,
        APPLICATION_ZIP,
        APPLICATION_OCTET_STREAM,
        APPLICATION_X_WWW_FORM_URLENCODED,
        // image/*
        IMAGE_PNG,
        IMAGE_JPEG,
        IMAGE_GIF,
        IMAGE_SVG_XML,
        IMAGE_WEBP,
        IMAGE_BMP,
        IMAGE_ICO,
        // video/*
        VIDEO_MP4,
        VIDEO_MPEG,
        VIDEO_WEBM,
        // audio/*
        AUDIO_MPEG,
        AUDIO_WAV,
        AUDIO_OGG,
        // font/*
        FONT_WOFF,
        FONT_WOFF2,
        FONT_TTF,
        FONT_OTF
    };

    // 2. コンストラクタ
    // デフォルトはUNKNOWN、またはEnumからの暗黙変換を許可
    ContentType(Type v = UNKNOWN) : type_(v) {}

    // 3. std::set のために必須 (オブジェクト同士の比較)
    bool operator<(const ContentType& other) const
    {
        return type_ < other.type_;
    }

    // 3. 一般的なロジックのために推奨 (オブジェクト同士の比較)
    bool operator==(const ContentType& other) const
    {
        return type_ == other.type_;
    }
    bool operator!=(const ContentType& other) const
    {
        return type_ != other.type_;
    }

    // 3. 曖昧さ回避のために推奨 (Enumとの直接比較)
    bool operator==(Type t) const { return type_ == t; }
    bool operator!=(Type t) const { return type_ != t; }

    // 4. Switch文で使えるようにするためのキャスト演算子
    operator Type() const { return type_; }

    // 5. 文字列化 (toString)
    // メモリ割り当てを避けるため、基本は const char* を返す
    const char* c_str() const
    {
        switch (type_)
        {
            case TEXT_HTML:
                return "text/html";
            case TEXT_PLAIN:
                return "text/plain";
            case TEXT_CSS:
                return "text/css";
            case TEXT_JAVASCRIPT:
                return "text/javascript";
            case TEXT_XML:
                return "text/xml";
            case TEXT_CSV:
                return "text/csv";
            case APPLICATION_JSON:
                return "application/json";
            case APPLICATION_XML:
                return "application/xml";
            case APPLICATION_PDF:
                return "application/pdf";
            case APPLICATION_ZIP:
                return "application/zip";
            case APPLICATION_OCTET_STREAM:
                return "application/octet-stream";
            case APPLICATION_X_WWW_FORM_URLENCODED:
                return "application/x-www-form-urlencoded";
            case IMAGE_PNG:
                return "image/png";
            case IMAGE_JPEG:
                return "image/jpeg";
            case IMAGE_GIF:
                return "image/gif";
            case IMAGE_SVG_XML:
                return "image/svg+xml";
            case IMAGE_WEBP:
                return "image/webp";
            case IMAGE_BMP:
                return "image/bmp";
            case IMAGE_ICO:
                return "image/x-icon";
            case VIDEO_MP4:
                return "video/mp4";
            case VIDEO_MPEG:
                return "video/mpeg";
            case VIDEO_WEBM:
                return "video/webm";
            case AUDIO_MPEG:
                return "audio/mpeg";
            case AUDIO_WAV:
                return "audio/wav";
            case AUDIO_OGG:
                return "audio/ogg";
            case FONT_WOFF:
                return "font/woff";
            case FONT_WOFF2:
                return "font/woff2";
            case FONT_TTF:
                return "font/ttf";
            case FONT_OTF:
                return "font/otf";
            default:
                return "application/octet-stream";
        }
    }

    // std::string が必要な場合のためのラッパー
    std::string toString() const { return std::string(c_str()); }

    // 6. 文字列からの生成 (fromMimeString)
    static ContentType fromMimeString(const char* mime_type)
    {
        if (mime_type == NULL)
            return UNKNOWN;

        if (std::strcmp(mime_type, "text/html") == 0)
            return TEXT_HTML;
        if (std::strcmp(mime_type, "text/plain") == 0)
            return TEXT_PLAIN;
        if (std::strcmp(mime_type, "text/css") == 0)
            return TEXT_CSS;
        if (std::strcmp(mime_type, "text/javascript") == 0)
            return TEXT_JAVASCRIPT;
        if (std::strcmp(mime_type, "text/xml") == 0)
            return TEXT_XML;
        if (std::strcmp(mime_type, "text/csv") == 0)
            return TEXT_CSV;
        if (std::strcmp(mime_type, "application/json") == 0)
            return APPLICATION_JSON;
        if (std::strcmp(mime_type, "application/xml") == 0)
            return APPLICATION_XML;
        if (std::strcmp(mime_type, "application/pdf") == 0)
            return APPLICATION_PDF;
        if (std::strcmp(mime_type, "application/zip") == 0)
            return APPLICATION_ZIP;
        if (std::strcmp(mime_type, "application/octet-stream") == 0)
            return APPLICATION_OCTET_STREAM;
        if (std::strcmp(mime_type, "application/x-www-form-urlencoded") == 0)
            return APPLICATION_X_WWW_FORM_URLENCODED;
        if (std::strcmp(mime_type, "image/png") == 0)
            return IMAGE_PNG;
        if (std::strcmp(mime_type, "image/jpeg") == 0)
            return IMAGE_JPEG;
        if (std::strcmp(mime_type, "image/gif") == 0)
            return IMAGE_GIF;
        if (std::strcmp(mime_type, "image/svg+xml") == 0)
            return IMAGE_SVG_XML;
        if (std::strcmp(mime_type, "image/webp") == 0)
            return IMAGE_WEBP;
        if (std::strcmp(mime_type, "image/bmp") == 0)
            return IMAGE_BMP;
        if (std::strcmp(mime_type, "image/x-icon") == 0)
            return IMAGE_ICO;
        if (std::strcmp(mime_type, "video/mp4") == 0)
            return VIDEO_MP4;
        if (std::strcmp(mime_type, "video/mpeg") == 0)
            return VIDEO_MPEG;
        if (std::strcmp(mime_type, "video/webm") == 0)
            return VIDEO_WEBM;
        if (std::strcmp(mime_type, "audio/mpeg") == 0)
            return AUDIO_MPEG;
        if (std::strcmp(mime_type, "audio/wav") == 0)
            return AUDIO_WAV;
        if (std::strcmp(mime_type, "audio/ogg") == 0)
            return AUDIO_OGG;
        if (std::strcmp(mime_type, "font/woff") == 0)
            return FONT_WOFF;
        if (std::strcmp(mime_type, "font/woff2") == 0)
            return FONT_WOFF2;
        if (std::strcmp(mime_type, "font/ttf") == 0)
            return FONT_TTF;
        if (std::strcmp(mime_type, "font/otf") == 0)
            return FONT_OTF;

        return UNKNOWN;
    }

    static ContentType fromMimeString(const std::string& mime_type)
    {
        return fromMimeString(mime_type.c_str());
    }

    // 7. 拡張子からの生成 (fromExtension)
    static ContentType fromExtension(const char* extension)
    {
        if (extension == NULL)
            return UNKNOWN;

        // 先頭の "." を削除
        const char* ext = extension;
        if (ext[0] == '.')
            ext++;

        if (std::strcmp(ext, "html") == 0 || std::strcmp(ext, "htm") == 0)
            return TEXT_HTML;
        if (std::strcmp(ext, "txt") == 0)
            return TEXT_PLAIN;
        if (std::strcmp(ext, "css") == 0)
            return TEXT_CSS;
        if (std::strcmp(ext, "js") == 0)
            return TEXT_JAVASCRIPT;
        if (std::strcmp(ext, "xml") == 0)
            return TEXT_XML;
        if (std::strcmp(ext, "csv") == 0)
            return TEXT_CSV;
        if (std::strcmp(ext, "json") == 0)
            return APPLICATION_JSON;
        if (std::strcmp(ext, "pdf") == 0)
            return APPLICATION_PDF;
        if (std::strcmp(ext, "zip") == 0)
            return APPLICATION_ZIP;
        if (std::strcmp(ext, "png") == 0)
            return IMAGE_PNG;
        if (std::strcmp(ext, "jpg") == 0 || std::strcmp(ext, "jpeg") == 0)
            return IMAGE_JPEG;
        if (std::strcmp(ext, "gif") == 0)
            return IMAGE_GIF;
        if (std::strcmp(ext, "svg") == 0)
            return IMAGE_SVG_XML;
        if (std::strcmp(ext, "webp") == 0)
            return IMAGE_WEBP;
        if (std::strcmp(ext, "bmp") == 0)
            return IMAGE_BMP;
        if (std::strcmp(ext, "ico") == 0)
            return IMAGE_ICO;
        if (std::strcmp(ext, "mp4") == 0)
            return VIDEO_MP4;
        if (std::strcmp(ext, "mpeg") == 0 || std::strcmp(ext, "mpg") == 0)
            return VIDEO_MPEG;
        if (std::strcmp(ext, "webm") == 0)
            return VIDEO_WEBM;
        if (std::strcmp(ext, "mp3") == 0)
            return AUDIO_MPEG;
        if (std::strcmp(ext, "wav") == 0)
            return AUDIO_WAV;
        if (std::strcmp(ext, "ogg") == 0)
            return AUDIO_OGG;
        if (std::strcmp(ext, "woff") == 0)
            return FONT_WOFF;
        if (std::strcmp(ext, "woff2") == 0)
            return FONT_WOFF2;
        if (std::strcmp(ext, "ttf") == 0)
            return FONT_TTF;
        if (std::strcmp(ext, "otf") == 0)
            return FONT_OTF;

        return APPLICATION_OCTET_STREAM;
    }

    static ContentType fromExtension(const std::string& extension)
    {
        return fromExtension(extension.c_str());
    }

    // 8. デフォルト値の取得
    static ContentType getDefault() { return APPLICATION_OCTET_STREAM; }

   private:
    Type type_;
};

}  // namespace http

#endif