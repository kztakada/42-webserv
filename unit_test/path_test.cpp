#include "utils/path.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

#include <string>

#include "utils/result.hpp"

TEST(Path, ResolveWithCwdRejectsEmptyPath)
{
    utils::result::Result<std::string> r =
        utils::path::resolvePhysicalPathWithCwd("", "/tmp");
    EXPECT_TRUE(r.isError());
}

TEST(Path, ResolveWithCwdRejectsNulInPath)
{
    std::string with_nul("a\0b", 3);
    utils::result::Result<std::string> r =
        utils::path::resolvePhysicalPathWithCwd(with_nul, "/tmp");
    EXPECT_TRUE(r.isError());
}

TEST(Path, ResolveWithCwdRejectsInvalidCwd)
{
    utils::result::Result<std::string> r1 =
        utils::path::resolvePhysicalPathWithCwd("a", "");
    EXPECT_TRUE(r1.isError());

    utils::result::Result<std::string> r2 =
        utils::path::resolvePhysicalPathWithCwd("a", "relative");
    EXPECT_TRUE(r2.isError());
}

TEST(Path, ResolveWithCwdNormalizesAbsolutePath)
{
    utils::result::Result<std::string> r =
        utils::path::resolvePhysicalPathWithCwd("/a//b///c", "/ignored");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/a/b/c", r.unwrap());

    r = utils::path::resolvePhysicalPathWithCwd("/a/./b", "/ignored");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/a/b", r.unwrap());

    r = utils::path::resolvePhysicalPathWithCwd("/a/b/..", "/ignored");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/a", r.unwrap());

    r = utils::path::resolvePhysicalPathWithCwd(
        "/a/b/../../../../c", "/ignored");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/c", r.unwrap());

    r = utils::path::resolvePhysicalPathWithCwd("/", "/ignored");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/", r.unwrap());
}

TEST(Path, ResolveWithCwdResolvesRelativePath)
{
    utils::result::Result<std::string> r =
        utils::path::resolvePhysicalPathWithCwd("./x//y", "/a/b");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/a/b/x/y", r.unwrap());

    r = utils::path::resolvePhysicalPathWithCwd("..", "/a/b");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/a", r.unwrap());

    r = utils::path::resolvePhysicalPathWithCwd(".", "/a/b");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/a/b", r.unwrap());
}

TEST(Path, ResolveWithCwdHandlesRootCwd)
{
    utils::result::Result<std::string> r =
        utils::path::resolvePhysicalPathWithCwd("a", "/");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/a", r.unwrap());

    r = utils::path::resolvePhysicalPathWithCwd("../a", "/");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/a", r.unwrap());
}

TEST(Path, PhysicalPathResolveWithCwdWrapsResolvedString)
{
    utils::result::Result<utils::path::PhysicalPath> r =
        utils::path::PhysicalPath::resolveWithCwd("./x/../y", "/a");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ("/a/y", r.unwrap().str());
    EXPECT_FALSE(r.unwrap().empty());
}

TEST(Path, PhysicalPathComparisonUsesNormalizedString)
{
    utils::result::Result<utils::path::PhysicalPath> a =
        utils::path::PhysicalPath::resolveWithCwd("a", "/");
    utils::result::Result<utils::path::PhysicalPath> b =
        utils::path::PhysicalPath::resolveWithCwd("b", "/");
    ASSERT_TRUE(a.isOk());
    ASSERT_TRUE(b.isOk());

    EXPECT_TRUE(a.unwrap() != b.unwrap());
    EXPECT_TRUE(a.unwrap() == a.unwrap());
    EXPECT_TRUE(a.unwrap() < b.unwrap());
}

TEST(Path, GetCurrentWorkingDirectoryReturnsAbsolutePath)
{
    utils::result::Result<std::string> r =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(r.isOk());

    const std::string cwd = r.unwrap();
    ASSERT_FALSE(cwd.empty());
    EXPECT_EQ('/', cwd[0]);

    // 末尾は '/' で終わらない(ルートは例外)
    if (cwd != "/")
    {
        EXPECT_NE('/', cwd[cwd.size() - 1]);
    }

    // 連続スラッシュは含まない
    EXPECT_EQ(std::string::npos, cwd.find("//"));
}

TEST(Path, GetCurrentWorkingDirectoryMatchesStdGetcwd)
{
    char buf[4096];
    ASSERT_TRUE(::getcwd(buf, sizeof(buf)) != NULL);
    const std::string expected(buf);

    utils::result::Result<std::string> r =
        utils::path::getCurrentWorkingDirectory();
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(expected, r.unwrap());
}
