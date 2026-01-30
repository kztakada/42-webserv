#ifndef WEBSERV_UTILS_PATH_HPP_
#define WEBSERV_UTILS_PATH_HPP_

#include <string>

#include "utils/result.hpp"

namespace utils
{
namespace path
{

class PhysicalPath
{
   public:
    PhysicalPath();

    // conf_spec.md 6章「物理パス系」の仕様に従い、入力パスを
    // 正規化された絶対パスとして解決して保持する。
    static utils::result::Result<PhysicalPath> resolve(
        const std::string& path_str);
    static utils::result::Result<PhysicalPath> resolveWithCwd(
        const std::string& path_str, const std::string& cwd);

    const std::string& str() const;
    bool empty() const;

   private:
    explicit PhysicalPath(const std::string& normalized_absolute_path);

    std::string normalized_absolute_path_;
};

bool operator==(const PhysicalPath& a, const PhysicalPath& b);
bool operator!=(const PhysicalPath& a, const PhysicalPath& b);
bool operator<(const PhysicalPath& a, const PhysicalPath& b);

// 現在の作業ディレクトリ(PWD)を絶対パスで返す。
// 禁止関数(getcwdなど)を使わず、chdir/stat/opendir/readdir で実装する。
utils::result::Result<std::string> getCurrentWorkingDirectory();

// conf_spec.md 6章「物理パス系」の仕様に従って、入力パスを
// - PWD基準で絶対パス化(相対パスの場合)
// - '//' の正規化(連続/を単一/へ)
// - '..' の解決(消去)
// した「正規化された絶対パス」として返す。
utils::result::Result<std::string> resolvePhysicalPath(
    const std::string& path_str);

// テスト用: PWDを外部から指定して同じ変換を行う。
utils::result::Result<std::string> resolvePhysicalPathWithCwd(
    const std::string& path_str, const std::string& cwd);

}  // namespace path
}  // namespace utils

#endif
