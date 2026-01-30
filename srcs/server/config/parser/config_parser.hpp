#ifndef WEBSERV_CONFIG_PARSER_HPP_
#define WEBSERV_CONFIG_PARSER_HPP_

#include <set>
#include <string>

#include "server/config/parser/conf_maker.hpp"
#include "server/config/server_config.hpp"
#include "utils/result.hpp"

namespace server
{

using namespace utils::result;
// コンフィグファイルのパーサー（静的メソッドのみ）
class ConfigParser
{
   public:
    // ファイルからConfigをパースする
    static Result<ServerConfig> parseFile(const std::string& filepath);

    // 文字列からConfigをパースする（テスト用）
    static Result<ServerConfig> parseData(const std::string& data);

   private:
    ServerConfig resultConfig_;
    void appendConf(const VirtualServerConf&
            virtual_server_conf);  // resultConfig_にVirtualServerConfを追加する

    // パース処理の内部状態を保持し、低レベルのトークンreader操作を担当するクラス
    class ParseContext
    {
       public:
        ParseContext(const std::string& content, size_t& idx,
            std::set<std::string>& directives)
            : file_content_(content),
              buf_idx_(idx),
              location_set_directives_(directives)
        {
        }

        // 1文字file_content_[buf_idx_]を返して buf_idx_ を1進める
        Result<char> getC();

        // buf_idx_ を1戻してfile_content_[buf_idx_]を返す
        char ungetC();

        // 空白文字以外が見つかるまで buf_idx_ を進める
        void skipSpaces();

        // getC() して返り値がセミコロンかどうかをboolで返す
        Result<bool> getCAndExpectSemicolon();

        // 次のスペース(改行等含む)までの文字列を取得し返す｡
        // そしてその文字数分 buf_idx_ を進める｡
        // 現在の buf_idx_ の位置が 'server {' の1文字目だった場合､ "server"
        // を返す｡
        Result<std::string> getWord();

        // file_content_ をすべて読み込んだか
        bool isEofReached() const;

        // location_set_directives_を参照し､そのディレクティブが今見ているlocation内で既にセットされたかどうかを返す
        bool isDirectiveSetInLocation(const std::string& directive) const;

        void clearLocationSetDirectives();
        void markDirectiveSetInLocation(const std::string& directive);

       private:
        const std::string& file_content_;
        size_t& buf_idx_;
        std::set<std::string>& location_set_directives_;
    };

    // ピリオドを含むドメイン全体の長さ
    static const int kMaxDomainLength = 253;
    // ドメインの各ラベル(ピリオド区切りの文字列)の最大長
    static const int kMaxDomainLabelLength = 63;
    // ポート番号の最大値
    static const unsigned long kMaxPortNumber = 65535;

    // 内部パース処理
    static Result<ServerConfig> parseConfigInternal(const std::string& data);

    // 文法の詳細は docks/configuration.g4 に書いてある｡

    // server block
    // server: 'server' '{' directive+ '}';
    static Result<void> parseServerBlock(
        ParseContext& ctx, ServerConfig& config);

    // listen_directive: 'listen' WHITESPACE NUMBER END_DIRECTIVE;
    static Result<void> parseListenDirective(
        ParseContext& ctx, VirtualServerConfMaker& vserver);

    // servername_directive: 'server_name' WHITESPACE DOMAIN_NAME+
    // END_DIRECTIVE;
    static Result<void> parseServerNameDirective(
        ParseContext& ctx, VirtualServerConfMaker& vserver);
    // location block
    // location_directive: 'location' PATH '{' directive_in_location+ '}';
    static Result<void> parseLocationBlock(ParseContext& ctx,
        LocationDirectiveConfMaker& vserver, bool is_location_back = false);

    static Result<void> parseAllowMethodDirective(
        ParseContext& ctx, LocationDirectiveConfMaker& location);
    static Result<void> parseCgiExecutorDirective(
        ParseContext& ctx, LocationDirectiveConfMaker& location);

    static Result<void> parseReturnDirective(
        ParseContext& ctx, LocationDirectiveConfMaker& location);

    // Parser utils

    // 符号なし整数かどうか
    static bool isDigits(const std::string& str);

    // ドメイン名の条件に合っているか
    // https://www.nic.ad.jp/ja/dom/system.html
    //
    // DOMAIN_NAME: DOMAIN_LABEL ('.' DOMAIN_LABEL)* [:PORT];
    static bool isDomainName(std::string domain_name);

    // ドメインのラベル(ドメイン内の'.'で区切られた文字列のこと)
    // DOMAIN_LABEL: (ALPHABET | NUMBER)+
    // 	| (ALPHABET | NUMBER)+ (ALPHABET | NUMBER | HYPHEN)* (
    // 		ALPHABET
    // 		| NUMBER
    // 	)+;
    static bool isDomainLabel(const std::string& label);

    // serverで利用可能なメソッドか
    // METHOD: 'GET' | 'POST' | 'DELETE';
    static bool isHttpMethod(const std::string& method);

    // 正しいHTTPステータスコードか
    static bool isValidHttpStatusCode(const std::string& code);

    // "<IP>:<Port>" もしくは "<Port>"
    static bool isValidListenArgument(const std::string& word);

    // <num>.<num>.<num>.<num> の形式か｡
    // num は 0~255 の数字
    static bool isValidIp(const std::string& ip);

    static std::pair<std::string, std::string> splitToIpAndPort(
        const std::string& ip_port);

    // portが符号なし整数であり､ポート番号の範囲に収まっているかチェックする
    static bool isValidPort(const std::string& port);

    // "on" なら true, "off" なら false を返す｡
    static Result<bool> parseOnOff(const std::string& on_or_off);

    // file_content_ をすべて読み込んだか
    static bool isEofReached(const ParseContext& ctx);

    // location_set_directives_を参照し､そのディレクティブが今見ているlocation内で既にセットされたかどうかを返す
    static bool isDirectiveSetInLocation(
        const ParseContext& ctx, const std::string& directive);
};

}  // namespace server

#endif