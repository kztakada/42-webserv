#ifndef WEBSERV_UTILS_PATH_HPP_
#define WEBSERV_UTILS_PATH_HPP_

#include <string>

#include "utils/result.hpp"

namespace utils
{
namespace path
{
using utils::result::Result;

class PhysicalPath
{
   public:
    PhysicalPath();

    // conf_spec.md 6章「物理パス系」の仕様に従い、入力パスを
    // 正規化された絶対パスとして解決して保持する。
    static Result<PhysicalPath> resolve(const std::string& path_str);
    static Result<PhysicalPath> resolveWithCwd(
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
Result<std::string> getCurrentWorkingDirectory();

// conf_spec.md 6章「物理パス系」の仕様に従って、入力パスを
// - PWD基準で絶対パス化(相対パスの場合)
// - '//' の正規化(連続/を単一/へ)
// - '..' の解決(消去)
// した「正規化された絶対パス」として返す。
Result<std::string> resolvePhysicalPath(const std::string& path_str);

// テスト用: PWDを外部から指定して同じ変換を行う。
Result<std::string> resolvePhysicalPathWithCwd(
    const std::string& path_str, const std::string& cwd);

// root_dir をドキュメントルートとして、uri_path(例: "/images/a.png") を
// 実ファイルシステム上で辿り、root_dir 配下に留まることを検証した上で
// アクセス対象の物理パスを返す。
// - symlink により途中で root_dir 外へ出る場合は ERROR を返す。
// - uri_path の各セグメントに '.' や '..' が含まれる場合は ERROR。
// - allow_nonexistent_leaf=true の場合、末尾要素が存在しなくても OK。
Result<PhysicalPath> resolvePhysicalPathUnderRoot(const PhysicalPath& root_dir,
    const std::string& uri_path, bool allow_nonexistent_leaf);

Result<PhysicalPath> resolvePhysicalPathUnderRoot(
    const PhysicalPath& root_dir, const std::string& uri_path);

}  // namespace path
}  // namespace utils

#endif
