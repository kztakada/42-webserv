#ifndef HTTP_CGI_RESPONSE_HPP_
#define HTTP_CGI_RESPONSE_HPP_

#include <map>
#include <string>
#include <vector>

#include "http/status.hpp"
#include "utils/byte.hpp"
#include "utils/result.hpp"

namespace http
{

using namespace utils::result;

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

    // CGI出力をパースする
    Result<void> parse(const std::vector<utils::Byte>& buffer);

    bool isParseComplete() const;
    CgiResponseType getResponseType() const;

    // パース結果の取得
    const HeaderVector& getHeaders() const;
    Result<std::string> getHeader(const std::string& name) const;
    const std::vector<utils::Byte>& getBody() const;

    // HTTPレスポンスへの変換（RFC 3875 Section 6.3）
    HttpStatus getHttpStatus() const;
    std::map<std::string, std::string> getHttpHeaders() const;

   private:
    CgiResponseType response_type_;
    HeaderVector headers_;  // 順序保持が必要
    std::vector<utils::Byte> body_;
    std::string newline_chars_;  // LF or CRLF
    bool parse_complete_;

    // RFC 3875 Section 6で定義された判定ロジック
    CgiResponseType determineResponseType() const;
    Result<void> validateCgiResponse() const;
};

}  // namespace http

#endif