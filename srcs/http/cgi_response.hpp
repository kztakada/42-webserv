#ifndef HTTP_CGI_RESPONSE_HPP_
#define HTTP_CGI_RESPONSE_HPP_

#include <string>
#include <vector>

#include "http/status.hpp"
#include "utils/data_type.hpp"
#include "utils/result.hpp"

namespace http
{

using namespace utils::result;

class HttpResponse;

// RFC 3875 Section 6で定義されたCGIレスポンスタイプ
enum CgiResponseType
{
    kNotIdentified,
    kDocumentResponse,            // RFC 3875 Section 6.2.1
    kLocalRedirect,               // RFC 3875 Section 6.2.2
    kClientRedirect,              // RFC 3875 Section 6.2.3
    kClientRedirectWithDocument,  // RFC 3875 Section 6.2.4
    kParseError
};

// RFC 3875 (CGI/1.1) 準拠のCGIレスポンスパーサー
class CgiResponse
{
   public:
    typedef std::pair<std::string, std::string> HeaderPair;
    typedef std::vector<HeaderPair> HeaderVector;

    CgiResponse();
    CgiResponse(const CgiResponse& rhs);
    CgiResponse& operator=(const CgiResponse& rhs);
    ~CgiResponse();

    // CGI出力（stdout）をストリーミングでパースする。
    // 戻り値: 入力 data のうち「ヘッダーとして消費したバイト数」。
    // ヘッダー終端（空行）を検出した時点で必ず止まり、残りはボディとして
    // 呼び出し側（セッション）が扱う。
    Result<size_t> parse(const utils::Byte* data, size_t len);

    bool isParseComplete() const;
    void reset();
    CgiResponseType getResponseType() const;

    // パース結果の取得
    const HeaderVector& getHeaders() const;
    Result<std::string> getHeader(const std::string& name) const;

    // Local Redirect の Location（"/path"）を返す
    Result<std::string> getLocalRedirectUriPath() const;

    // HTTPレスポンスへの変換（RFC 3875 Section 6.3）
    HttpStatus getHttpStatus() const;
    // out_response に status とヘッダを適用する（ボディは扱わない）
    Result<void> applyToHttpResponse(http::HttpResponse& out_response) const;

   private:
    enum Phase
    {
        kHeader,
        kComplete,
        kError
    };

    CgiResponseType response_type_;
    HeaderVector headers_;  // 順序保持が必要
    Phase phase_;
    std::string line_buffer_;  // 行が分割された場合の保持

    // RFC 3875 Section 6で定義された判定ロジック
    CgiResponseType determineResponseType() const;
    Result<void> validateCgiResponse() const;

    static bool ieq_(const std::string& a, const std::string& b);
    static std::string trimOws_(const std::string& s);
    Result<void> parseHeaderLine_(const std::string& line);
    Result<void> finalize_();
};

}  // namespace http

#endif
