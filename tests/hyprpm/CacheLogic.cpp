#include "../../hyprpm/src/core/CacheLogic.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <format>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace NCacheLogic;

namespace {
    std::string shellQuote(std::string_view value) {
        std::string result = "'";
        for (const auto c : value)
            result += c == '\'' ? "'\\''" : std::string{c};
        return result + "'";
    }

    void runCommand(const std::string& command) {
        const auto STATUS = std::system(command.c_str());
        ASSERT_NE(STATUS, -1);
        ASSERT_TRUE(WIFEXITED(STATUS));
        ASSERT_EQ(WEXITSTATUS(STATUS), 0);
    }

    std::string readHead(const std::filesystem::path& repository, const std::filesystem::path& resultFile) {
        runCommand(std::format("git -C {} rev-parse HEAD > {}", shellQuote(repository.string()), shellQuote(resultFile.string())));
        std::ifstream stream{resultFile};
        std::string   hash;
        stream >> hash;
        return hash;
    }

    class CCacheLogicTest : public testing::Test {
      protected:
        void SetUp() override {
            auto pathTemplate = (std::filesystem::temp_directory_path() / "hyprpm-cache-test-XXXXXX").string();
            pathTemplate.push_back('\0');
            const auto PATH = mkdtemp(pathTemplate.data());
            ASSERT_NE(PATH, nullptr);
            m_path = PATH;
        }

        void TearDown() override {
            std::error_code ec;
            std::filesystem::remove_all(m_path, ec);
        }

        std::filesystem::path m_path;
    };
}

TEST(HyprpmCacheLogic, KeepsDefaultWorkingDirectory) {
    const auto                 CACHE_ROOT = std::filesystem::path{"/cache/hyprpm/repos"};
    const auto                 TEMP_ROOT  = std::filesystem::path{"/tmp/hyprpm"};
    constexpr std::string_view URL        = "https://example.com/owner/plugin";

    EXPECT_EQ(workingRepositoryPath(false, CACHE_ROOT, TEMP_ROOT, "alice", URL), TEMP_ROOT / "alice");
    EXPECT_EQ(workingRepositoryPath(true, CACHE_ROOT, TEMP_ROOT, "alice", URL), repositoryPath(CACHE_ROOT, URL));
}

TEST(HyprpmCacheLogic, UsesStableUrlCacheKey) {
    const auto                 CACHE_ROOT = std::filesystem::path{"/cache/hyprpm/repos"};
    constexpr std::string_view URL        = "https://example.com/owner/plugin";

    EXPECT_EQ(repositoryPath(CACHE_ROOT, URL), CACHE_ROOT / "6be41d975c612477");
    EXPECT_NE(repositoryPath(CACHE_ROOT, URL), repositoryPath(CACHE_ROOT, "https://other.example/owner/plugin"));
}

TEST(HyprpmCacheLogic, DoesNotRebuildUnchangedPinnedRevision) {
    EXPECT_FALSE(repositoryNeedsUpdate(false, "fixed-commit", "fixed-commit"));
    EXPECT_TRUE(repositoryNeedsUpdate(false, "new-commit", "old-commit"));
    EXPECT_TRUE(repositoryNeedsUpdate(true, "fixed-commit", "fixed-commit"));
}

TEST_F(CCacheLogicTest, RemoteHeadMovementDoesNotRebuildPinnedRevision) {
    const auto REMOTE = m_path / "remote.git";
    const auto SOURCE = m_path / "source";
    const auto CACHE  = m_path / "cache";
    const auto RESULT = m_path / "head";

    runCommand(std::format("git init --bare {} && git init {} && git -C {} config user.name test && git -C {} config user.email test@example.com", shellQuote(REMOTE.string()),
                           shellQuote(SOURCE.string()), shellQuote(SOURCE.string()), shellQuote(SOURCE.string())));
    {
        std::ofstream file{SOURCE / "plugin.cpp"};
        file << "A";
    }
    runCommand(std::format("git -C {} add plugin.cpp && git -C {} commit -m A && git -C {} remote add origin {} && git -C {} push -u origin HEAD", shellQuote(SOURCE.string()),
                           shellQuote(SOURCE.string()), shellQuote(SOURCE.string()), shellQuote(REMOTE.string()), shellQuote(SOURCE.string())));
    const auto PINNED_HASH = readHead(SOURCE, RESULT);

    runCommand(std::format("git clone {} {}", shellQuote(REMOTE.string()), shellQuote(CACHE.string())));
    {
        std::ofstream file{SOURCE / "plugin.cpp"};
        file << "B";
    }
    runCommand(std::format("git -C {} commit -am B && git -C {} push", shellQuote(SOURCE.string()), shellQuote(SOURCE.string())));

    runCommand(std::format("git -C {} fetch origin && git -C {} reset --hard origin/HEAD", shellQuote(CACHE.string()), shellQuote(CACHE.string())));
    const auto REMOTE_HEAD = readHead(CACHE, RESULT);
    ASSERT_NE(REMOTE_HEAD, PINNED_HASH);

    runCommand(checkoutRepositoryRevisionCommand(CACHE, PINNED_HASH) + " > " + shellQuote(RESULT.string()));
    std::ifstream checkedOutStream{RESULT};
    std::string   CHECKED_OUT_HASH;
    checkedOutStream >> CHECKED_OUT_HASH;
    ASSERT_EQ(CHECKED_OUT_HASH, PINNED_HASH);
    EXPECT_FALSE(repositoryNeedsUpdate(false, CHECKED_OUT_HASH, PINNED_HASH));
    EXPECT_TRUE(repositoryNeedsUpdate(true, CHECKED_OUT_HASH, PINNED_HASH));
}

TEST_F(CCacheLogicTest, NormalizesOutputPathUsedForInstallation) {
    std::filesystem::create_directories(m_path / "build");

    const auto OUTPUT = pluginOutputPath(m_path, "build/../build/plugin.so");
    ASSERT_TRUE(OUTPUT);
    EXPECT_EQ(*OUTPUT, m_path / "build/plugin.so");
}

TEST_F(CCacheLogicTest, RejectsOutputPathEscapes) {
    EXPECT_FALSE(pluginOutputPath(m_path, "../plugin.so"));
    EXPECT_FALSE(pluginOutputPath(m_path, "."));
    EXPECT_FALSE(pluginOutputPath(m_path, "/tmp/plugin.so"));

    auto outsideTemplate = (std::filesystem::temp_directory_path() / "hyprpm-cache-outside-XXXXXX").string();
    outsideTemplate.push_back('\0');
    const auto OUTSIDE = mkdtemp(outsideTemplate.data());
    ASSERT_NE(OUTSIDE, nullptr);
    const auto OUTSIDE_PATH = std::filesystem::path{OUTSIDE};

    std::filesystem::create_directory_symlink(OUTSIDE_PATH, m_path / "escape");
    EXPECT_FALSE(pluginOutputPath(m_path, "escape/plugin.so"));

    std::error_code ec;
    std::filesystem::remove_all(OUTSIDE_PATH, ec);
}

TEST_F(CCacheLogicTest, OpensOnlyOwnedRegularPluginOutput) {
    const auto OUTPUT = m_path / "plugin.so";
    {
        std::ofstream file{OUTPUT};
        file << "original";
    }

    auto sourceFD = openValidatedPluginBinary(OUTPUT, getuid());
    ASSERT_TRUE(sourceFD.isValid());

    struct stat openedStat;
    ASSERT_EQ(fstat(sourceFD.get(), &openedStat), 0);

    std::filesystem::rename(OUTPUT, m_path / "original.so");
    {
        std::ofstream replacement{OUTPUT};
        replacement << "replacement";
    }

    struct stat originalStat;
    struct stat replacementStat;
    ASSERT_EQ(stat((m_path / "original.so").c_str(), &originalStat), 0);
    ASSERT_EQ(stat(OUTPUT.c_str(), &replacementStat), 0);
    EXPECT_EQ(openedStat.st_ino, originalStat.st_ino);
    EXPECT_NE(openedStat.st_ino, replacementStat.st_ino);

    std::filesystem::remove(OUTPUT);
    EXPECT_FALSE(openValidatedPluginBinary(OUTPUT, getuid()).isValid());

    std::filesystem::create_symlink(m_path / "original.so", OUTPUT);
    EXPECT_FALSE(openValidatedPluginBinary(OUTPUT, getuid()).isValid());
}
