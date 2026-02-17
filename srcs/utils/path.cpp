#include "utils/path.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

namespace utils
{
namespace path
{

using utils::result::ERROR;
using utils::result::OK;
using utils::result::Result;

static bool containsNul_(const std::string& s)
{
    return s.find('\0') != std::string::npos;
}

static Result<void> chdirDown_(const std::vector<std::string>& leaf_to_root)
{
    // 現在地(祖先)から元のディレクトリへ降りる。
    for (std::vector<std::string>::const_reverse_iterator it =
             leaf_to_root.rbegin();
        it != leaf_to_root.rend(); ++it)
    {
        if (::chdir(it->c_str()) != 0)
        {
            return Result<void>(ERROR, "failed to restore working directory");
        }
    }
    return Result<void>();
}

Result<std::string> getCurrentWorkingDirectory()
{
    // 現在の場所からルートまで登り、各親ディレクトリから見た子の名前を集めて
    // 逆順に連結して絶対パスを作る。
    std::vector<std::string> components;

    while (true)
    {
        struct stat cur;
        struct stat parent;

        if (::stat(".", &cur) != 0)
            return Result<std::string>(ERROR, std::string(), "stat(.) failed");

        if (::stat("..", &parent) != 0)
            return Result<std::string>(ERROR, std::string(), "stat(..) failed");

        // ルートディレクトリでは '.' と '..' が同じ inode/dev になる
        if (cur.st_ino == parent.st_ino && cur.st_dev == parent.st_dev)
            break;

        DIR* dp = ::opendir("..");
        if (dp == NULL)
        {
            Result<void> restored = chdirDown_(components);
            (void)restored;
            return Result<std::string>(
                ERROR, std::string(), "opendir(..) failed");
        }

        std::string child_name;
        for (struct dirent* ent = ::readdir(dp); ent != NULL;
            ent = ::readdir(dp))
        {
            // 親ディレクトリのエントリを、現在地から "../name" として stat する
            std::string candidate = std::string("../") + ent->d_name;
            struct stat st;
            if (::stat(candidate.c_str(), &st) != 0)
                continue;

            if (st.st_ino == cur.st_ino && st.st_dev == cur.st_dev)
            {
                child_name = ent->d_name;
                break;
            }
        }
        ::closedir(dp);

        if (child_name.empty())
        {
            Result<void> restored = chdirDown_(components);
            (void)restored;
            return Result<std::string>(
                ERROR, std::string(), "failed to find cwd component");
        }
        components.push_back(child_name);

        if (::chdir("..") != 0)
        {
            Result<void> restored = chdirDown_(components);
            (void)restored;
            return Result<std::string>(
                ERROR, std::string(), "chdir(..) failed");
        }
    }

    // 現在地はルート。絶対パスを組み立てる。
    std::string abs_path = "/";
    for (std::vector<std::string>::reverse_iterator it = components.rbegin();
        it != components.rend(); ++it)
    {
        abs_path += *it;
        if (it + 1 != components.rend())
            abs_path += "/";
    }

    // 元のディレクトリへ戻す
    Result<void> restored = chdirDown_(components);
    if (restored.isError())
    {
        return Result<std::string>(
            ERROR, std::string(), restored.getErrorMessage());
    }
    return Result<std::string>(abs_path);
}

static Result<std::string> normalizePhysicalAbsolutePath_(
    const std::string& abs_path)
{
    if (abs_path.empty() || abs_path[0] != '/')
    {
        return Result<std::string>(
            ERROR, std::string(), "physical path must be absolute");
    }
    // 連続/・'.'・'..' を解決する（lexical）
    std::vector<std::string> parts;
    std::string token;

    for (size_t i = 0; i <= abs_path.size(); ++i)
    {
        char c = (i < abs_path.size()) ? abs_path[i] : '/';
        if (c == '/')
        {
            if (token.empty() || token == ".")
            {
                // skip
            }
            else if (token == "..")
            {
                if (!parts.empty())
                {
                    parts.pop_back();
                }
                // ルートより上は切り捨て
            }
            else
            {
                parts.push_back(token);
            }
            token.clear();
        }
        else
        {
            token += c;
        }
    }

    std::string out = "/";
    for (size_t i = 0; i < parts.size(); ++i)
    {
        out += parts[i];
        if (i + 1 < parts.size())
            out += "/";
    }
    return Result<std::string>(out);
}

static bool isUnderRootPhysical_(
    const std::string& root_physical, const std::string& cur_physical)
{
    if (root_physical.empty() || root_physical[0] != '/')
        return false;

    if (root_physical == "/")
        return !cur_physical.empty() && cur_physical[0] == '/';

    if (cur_physical == root_physical)
        return true;

    if (cur_physical.size() <= root_physical.size())
        return false;

    if (cur_physical.compare(0, root_physical.size(), root_physical) != 0)
        return false;

    return cur_physical[root_physical.size()] == '/';
}

static Result<void> validateUriPathForUnderRoot_(const std::string& uri_path)
{
    if (uri_path.empty() || uri_path[0] != '/')
        return Result<void>(ERROR, "uri path must start with '/'");

    if (containsNul_(uri_path))
        return Result<void>(ERROR, "uri path contains NUL");

    return Result<void>();
}

static std::vector<std::string> splitUriSegments_(const std::string& uri_path)
{
    std::vector<std::string> out;
    std::string token;
    for (size_t i = 0; i <= uri_path.size(); ++i)
    {
        const char c = (i < uri_path.size()) ? uri_path[i] : '/';
        if (c == '/')
        {
            if (!token.empty())
                out.push_back(token);

            token.clear();
        }
        else
        {
            token += c;
        }
    }
    return out;
}

Result<PhysicalPath> resolvePhysicalPathUnderRoot(const PhysicalPath& root_dir,
    const std::string& uri_path, bool allow_nonexistent_leaf)
{
    if (root_dir.empty())
        return Result<PhysicalPath>(ERROR, "root_dir is empty");

    Result<void> v = validateUriPathForUnderRoot_(uri_path);
    if (v.isError())
        return Result<PhysicalPath>(ERROR, v.getErrorMessage());

    // 元の cwd を保存
    Result<std::string> original_cwd = getCurrentWorkingDirectory();
    if (original_cwd.isError())
        return Result<PhysicalPath>(ERROR, "failed to get cwd");

    // root_dir へ移動し、root の物理パスを確定
    if (::chdir(root_dir.str().c_str()) != 0)
        return Result<PhysicalPath>(ERROR, "failed to chdir to root_dir");

    Result<std::string> root_physical = getCurrentWorkingDirectory();
    if (root_physical.isError())
    {
        if (::chdir(original_cwd.unwrap().c_str()) != 0) { (void)0; }
        return Result<PhysicalPath>(ERROR, "failed to get physical root cwd");
    }

    std::vector<std::string> segments = splitUriSegments_(uri_path);

    for (size_t i = 0; i < segments.size(); ++i)
    {
        const std::string& seg = segments[i];
        const bool is_last = (i + 1 == segments.size());

        if (seg == "." || seg == "..")
        {
            if (::chdir(original_cwd.unwrap().c_str()) != 0) { (void)0; }
            return Result<PhysicalPath>(ERROR, "uri path contains dot segment");
        }

        struct stat st;
        if (::stat(seg.c_str(), &st) != 0)
        {
            if (allow_nonexistent_leaf && is_last)
            {
                Result<std::string> cur_physical = getCurrentWorkingDirectory();
                if (::chdir(original_cwd.unwrap().c_str()) != 0) { (void)0; }
                if (cur_physical.isError() ||
                    !isUnderRootPhysical_(
                        root_physical.unwrap(), cur_physical.unwrap()))
                {
                    return Result<PhysicalPath>(
                        ERROR, "path escapes root_dir (physical)");
                }
                // leaf は存在しないが、親ディレクトリが root
                // 配下であることは保証済み
                Result<PhysicalPath> leaf =
                    PhysicalPath::resolveWithCwd(seg, cur_physical.unwrap());
                if (leaf.isError())
                    return Result<PhysicalPath>(ERROR, leaf.getErrorMessage());

                return leaf;
            }

            if (::chdir(original_cwd.unwrap().c_str()) != 0) { (void)0; }
            return Result<PhysicalPath>(ERROR, "path component does not exist");
        }

        const bool is_dir = (st.st_mode & S_IFDIR) != 0;
        if (is_dir)
        {
            if (::chdir(seg.c_str()) != 0)
            {
                if (::chdir(original_cwd.unwrap().c_str()) != 0) { (void)0; }
                return Result<PhysicalPath>(ERROR, "failed to chdir component");
            }
            Result<std::string> cur_physical = getCurrentWorkingDirectory();
            if (cur_physical.isError() ||
                !isUnderRootPhysical_(
                    root_physical.unwrap(), cur_physical.unwrap()))
            {
                if (::chdir(original_cwd.unwrap().c_str()) != 0) { (void)0; }
                return Result<PhysicalPath>(
                    ERROR, "path escapes root_dir (physical)");
            }
            continue;
        }

        // ファイルなど (非ディレクトリ)
        if (!is_last)
        {
            if (::chdir(original_cwd.unwrap().c_str()) != 0) { (void)0; }
            return Result<PhysicalPath>(
                ERROR, "non-directory component in the middle of path");
        }

        Result<std::string> cur_physical = getCurrentWorkingDirectory();
        if (::chdir(original_cwd.unwrap().c_str()) != 0) { (void)0; }
        if (cur_physical.isError() ||
            !isUnderRootPhysical_(
                root_physical.unwrap(), cur_physical.unwrap()))
        {
            return Result<PhysicalPath>(
                ERROR, "path escapes root_dir (physical)");
        }
        Result<PhysicalPath> leaf =
            PhysicalPath::resolveWithCwd(seg, cur_physical.unwrap());
        if (leaf.isError())
            return Result<PhysicalPath>(ERROR, leaf.getErrorMessage());

        return leaf;
    }

    // ここに到達するのは、uri_path == "/" もしくは
    // 末尾までがディレクトリとして辿れたケース。
    Result<std::string> cur_physical = getCurrentWorkingDirectory();
    if (::chdir(original_cwd.unwrap().c_str()) != 0) { (void)0; }
    if (cur_physical.isError() ||
        !isUnderRootPhysical_(root_physical.unwrap(), cur_physical.unwrap()))
        return Result<PhysicalPath>(ERROR, "path escapes root_dir (physical)");

    Result<PhysicalPath> cur_as_path =
        PhysicalPath::resolve(cur_physical.unwrap());
    if (cur_as_path.isError())
        return Result<PhysicalPath>(ERROR, cur_as_path.getErrorMessage());

    return cur_as_path;
}

Result<PhysicalPath> resolvePhysicalPathUnderRoot(
    const PhysicalPath& root_dir, const std::string& uri_path)
{
    return resolvePhysicalPathUnderRoot(root_dir, uri_path, false);
}

Result<std::string> resolvePhysicalPathWithCwd(
    const std::string& path_str, const std::string& cwd)
{
    if (path_str.empty())
        return Result<std::string>(ERROR, std::string(), "path is empty");

    if (containsNul_(path_str))
        return Result<std::string>(ERROR, std::string(), "path contains NUL");

    if (cwd.empty() || cwd[0] != '/')
        return Result<std::string>(ERROR, std::string(), "cwd is invalid");

    std::string abs;
    if (!path_str.empty() && path_str[0] == '/')
    {
        abs = path_str;
    }
    else
    {
        // cwd は正規化済み absolute を想定
        if (cwd == "/")
        {
            abs = "/" + path_str;
        }
        else
        {
            abs = cwd + "/" + path_str;
        }
    }
    return normalizePhysicalAbsolutePath_(abs);
}

Result<std::string> resolvePhysicalPath(const std::string& path_str)
{
    Result<std::string> cwd = getCurrentWorkingDirectory();
    if (cwd.isError())
        return Result<std::string>(ERROR, std::string(), "failed to get cwd");

    return resolvePhysicalPathWithCwd(path_str, cwd.unwrap());
}

PhysicalPath::PhysicalPath() : normalized_absolute_path_() {}

PhysicalPath::PhysicalPath(const std::string& normalized_absolute_path)
    : normalized_absolute_path_(normalized_absolute_path)
{
}

Result<PhysicalPath> PhysicalPath::resolve(const std::string& path_str)
{
    Result<std::string> resolved = resolvePhysicalPath(path_str);
    if (resolved.isError())
    {
        return Result<PhysicalPath>(
            ERROR, PhysicalPath(), resolved.getErrorMessage());
    }
    return PhysicalPath(resolved.unwrap());
}

Result<PhysicalPath> PhysicalPath::resolveWithCwd(
    const std::string& path_str, const std::string& cwd)
{
    Result<std::string> resolved = resolvePhysicalPathWithCwd(path_str, cwd);
    if (resolved.isError())
    {
        return Result<PhysicalPath>(
            ERROR, PhysicalPath(), resolved.getErrorMessage());
    }
    return PhysicalPath(resolved.unwrap());
}

const std::string& PhysicalPath::str() const
{
    return normalized_absolute_path_;
}

bool PhysicalPath::empty() const { return normalized_absolute_path_.empty(); }

bool operator==(const PhysicalPath& a, const PhysicalPath& b)
{
    return a.str() == b.str();
}

bool operator!=(const PhysicalPath& a, const PhysicalPath& b)
{
    return !(a == b);
}

bool operator<(const PhysicalPath& a, const PhysicalPath& b)
{
    return a.str() < b.str();
}

}  // namespace path
}  // namespace utils
