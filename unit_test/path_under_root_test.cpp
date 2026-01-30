#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

#include "utils/path.hpp"

static std::string makeTempDirOrDie_()
{
    char tmpl[] = "/tmp/webserv_under_root_test.XXXXXX";
    char* p = ::mkdtemp(tmpl);
    EXPECT_TRUE(p != NULL);
    return std::string(p ? p : "");
}

static void mkdirOrDie_(const std::string& path)
{
    int rc = ::mkdir(path.c_str(), 0700);
    if (rc != 0)
    {
        // 既に存在する場合はOK
        ASSERT_TRUE(errno == EEXIST);
    }
}

static void symlinkOrDie_(
    const std::string& target, const std::string& linkpath)
{
    int rc = ::symlink(target.c_str(), linkpath.c_str());
    ASSERT_EQ(rc, 0);
}

static void touchFileOrDie_(const std::string& path)
{
    FILE* fp = ::fopen(path.c_str(), "w");
    ASSERT_TRUE(fp != NULL);
    ::fputs("x", fp);
    ::fclose(fp);
}

TEST(PathUnderRoot, AllowsNormalPath)
{
    const std::string base = makeTempDirOrDie_();
    const std::string root = base + "/root";
    mkdirOrDie_(root);
    mkdirOrDie_(root + "/dir");
    touchFileOrDie_(root + "/dir/file.txt");

    utils::result::Result<utils::path::PhysicalPath> root_p =
        utils::path::PhysicalPath::resolve(root);
    ASSERT_TRUE(root_p.isOk());

    utils::result::Result<utils::path::PhysicalPath> p =
        utils::path::resolvePhysicalPathUnderRoot(
            root_p.unwrap(), "/dir/file.txt", true);
    ASSERT_TRUE(p.isOk());

    EXPECT_EQ(p.unwrap().str(), root + "/dir/file.txt");
}

TEST(PathUnderRoot, RejectsSymlinkDirectoryEscape)
{
    const std::string base = makeTempDirOrDie_();
    const std::string root = base + "/root";
    const std::string outside = base + "/outside";

    mkdirOrDie_(root);
    mkdirOrDie_(outside);
    touchFileOrDie_(outside + "/secret.txt");

    // root/link -> ../outside
    symlinkOrDie_("../outside", root + "/link");

    utils::result::Result<utils::path::PhysicalPath> root_p =
        utils::path::PhysicalPath::resolve(root);
    ASSERT_TRUE(root_p.isOk());

    utils::result::Result<utils::path::PhysicalPath> p =
        utils::path::resolvePhysicalPathUnderRoot(
            root_p.unwrap(), "/link/secret.txt", true);
    EXPECT_TRUE(p.isError());
}

TEST(PathUnderRoot, RejectsDotDotSegments)
{
    const std::string base = makeTempDirOrDie_();
    const std::string root = base + "/root";
    mkdirOrDie_(root);
    mkdirOrDie_(root + "/dir");

    utils::result::Result<utils::path::PhysicalPath> root_p =
        utils::path::PhysicalPath::resolve(root);
    ASSERT_TRUE(root_p.isOk());

    utils::result::Result<utils::path::PhysicalPath> p =
        utils::path::resolvePhysicalPathUnderRoot(
            root_p.unwrap(), "/dir/../x", true);
    EXPECT_TRUE(p.isError());
}
