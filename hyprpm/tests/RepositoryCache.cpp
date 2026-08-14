#include "../src/core/RepositoryCache.hpp"

#include <fstream>
#include <format>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

class CRepositoryCacheTest : public testing::Test {
  protected:
    void SetUp() override {
        auto path = (std::filesystem::temp_directory_path() / "hyprpm-cache-test-XXXXXX").string();
        ASSERT_NE(mkdtemp(path.data()), nullptr);
        m_root = path;
        std::filesystem::create_directories(m_root / "build");
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_root, ec);
    }

    std::filesystem::path m_root;
};

TEST(RepositoryCache, keyIsStableAndUrlSpecific) {
    EXPECT_EQ(NRepositoryCache::key("https://example.com/plugin.git"), 0x08684eeac71708a9ULL);
    EXPECT_NE(NRepositoryCache::key("https://example.com/plugin.git"), NRepositoryCache::key("https://example.com/other.git"));
}

TEST_F(CRepositoryCacheTest, secureDirectoryRejectsUnsafeModesAndOwners) {
    ASSERT_EQ(chmod(m_root.c_str(), S_IRWXU), 0);
    EXPECT_TRUE(NRepositoryCache::secureDirectory(m_root, getuid()));
    EXPECT_FALSE(NRepositoryCache::secureDirectory(m_root, getuid() + 1));

    ASSERT_EQ(chmod(m_root.c_str(), S_IRWXU | S_IWGRP), 0);
    EXPECT_FALSE(NRepositoryCache::secureDirectory(m_root, getuid()));
}

TEST_F(CRepositoryCacheTest, resolvePathWithinAcceptsNestedOutput) {
    const auto OUTPUT = NRepositoryCache::resolvePathWithin(m_root, "build/plugin.so");
    ASSERT_TRUE(OUTPUT.has_value());
    EXPECT_EQ(*OUTPUT, m_root / "build/plugin.so");
}

TEST_F(CRepositoryCacheTest, resolvePathWithinRejectsTraversalAndAbsolutePaths) {
    EXPECT_FALSE(NRepositoryCache::resolvePathWithin(m_root, "../plugin.so").has_value());
    EXPECT_FALSE(NRepositoryCache::resolvePathWithin(m_root, m_root.parent_path() / "plugin.so").has_value());
    EXPECT_FALSE(NRepositoryCache::resolvePathWithin(m_root, ".").has_value());
}

TEST_F(CRepositoryCacheTest, resolvePathWithinAllowsInTreeSymlinks) {
    std::ofstream{m_root / "build/plugin-real.so"} << "plugin";
    std::filesystem::create_symlink("plugin-real.so", m_root / "build/plugin.so");

    const auto OUTPUT = NRepositoryCache::resolvePathWithin(m_root, "build/plugin.so");
    ASSERT_TRUE(OUTPUT.has_value());
    EXPECT_EQ(*OUTPUT, m_root / "build/plugin-real.so");
    EXPECT_TRUE(NRepositoryCache::regularFileOwnedBy(*OUTPUT, getuid()));
}

TEST_F(CRepositoryCacheTest, resolvePathSlotWithinKeepsFinalSymlink) {
    std::ofstream{m_root / "build/plugin-real.so"} << "plugin";
    std::filesystem::create_symlink("plugin-real.so", m_root / "build/plugin.so");

    const auto OUTPUT = NRepositoryCache::resolvePathSlotWithin(m_root, "build/plugin.so");
    ASSERT_TRUE(OUTPUT.has_value());
    EXPECT_EQ(*OUTPUT, m_root / "build/plugin.so");
    EXPECT_TRUE(std::filesystem::is_symlink(std::filesystem::symlink_status(*OUTPUT)));
}

TEST_F(CRepositoryCacheTest, resolvePathWithinRejectsOutsideSymlinks) {
    const auto OUTSIDE = m_root.parent_path() / std::format("{}-outside.so", m_root.filename().string());
    std::ofstream{OUTSIDE} << "plugin";
    std::filesystem::create_symlink(OUTSIDE, m_root / "build/plugin.so");

    EXPECT_FALSE(NRepositoryCache::resolvePathWithin(m_root, "build/plugin.so").has_value());

    std::error_code ec;
    std::filesystem::remove(OUTSIDE, ec);
}

TEST_F(CRepositoryCacheTest, resolvePathSlotWithinRejectsOutsideParentSymlinks) {
    const auto OUTSIDE = m_root.parent_path() / std::format("{}-outside", m_root.filename().string());
    std::filesystem::create_directory(OUTSIDE);
    std::filesystem::create_directory_symlink(OUTSIDE, m_root / "outside");

    EXPECT_FALSE(NRepositoryCache::resolvePathSlotWithin(m_root, "outside/plugin.so").has_value());

    std::error_code ec;
    std::filesystem::remove_all(OUTSIDE, ec);
}
