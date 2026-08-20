#include "CacheLogic.hpp"

#include <cstdint>
#include <format>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace NCacheLogic {
    static std::string shellQuote(std::string_view value) {
        std::string result = "'";
        for (const auto c : value)
            result += c == '\'' ? "'\\''" : std::string{c};
        return result + "'";
    }

    // Keyed by URL so repositories sharing a name do not collide. FNV-1a is used instead of std::hash because this key is persisted on disk.
    std::filesystem::path repositoryPath(const std::filesystem::path& cacheRoot, std::string_view url) {
        uint64_t hash = 14695981039346656037ULL;

        for (const auto c : url) {
            hash ^= static_cast<uint8_t>(c);
            hash *= 1099511628211ULL;
        }

        return cacheRoot / std::format("{:016x}", hash);
    }

    std::filesystem::path workingRepositoryPath(bool experimentalCache, const std::filesystem::path& cacheRoot, const std::filesystem::path& tempRoot, std::string_view username,
                                                std::string_view url) {
        return experimentalCache ? repositoryPath(cacheRoot, url) : tempRoot / username;
    }

    bool repositoryNeedsUpdate(bool forceUpdate, std::string_view checkedOutHash, std::string_view storedHash) {
        return forceUpdate || checkedOutHash != storedHash;
    }

    std::string checkoutRepositoryRevisionCommand(const std::filesystem::path& repository, std::string_view revision) {
        const auto REPOSITORY = shellQuote(repository.string());
        if (revision.empty())
            return std::format("git -C {} rev-parse HEAD", REPOSITORY);

        return std::format("git -C {} reset --quiet --hard --recurse-submodules {} && git -C {} rev-parse HEAD", REPOSITORY, shellQuote(revision), REPOSITORY);
    }

    std::optional<std::filesystem::path> pluginOutputPath(const std::filesystem::path& repositoryRoot, std::string_view output) {
        const auto RELATIVE_OUTPUT = std::filesystem::path{output}.lexically_normal();
        if (RELATIVE_OUTPUT.empty() || RELATIVE_OUTPUT == "." || RELATIVE_OUTPUT.is_absolute() || *RELATIVE_OUTPUT.begin() == "..")
            return std::nullopt;

        std::error_code ec;
        const auto      ROOT = std::filesystem::canonical(repositoryRoot, ec);
        if (ec)
            return std::nullopt;

        const auto OUTPUT_PATH = ROOT / RELATIVE_OUTPUT;
        const auto PARENT      = std::filesystem::weakly_canonical(OUTPUT_PATH.parent_path(), ec);
        if (ec || OUTPUT_PATH == ROOT)
            return std::nullopt;

        const auto RESULT   = PARENT / OUTPUT_PATH.filename();
        const auto RELATIVE = PARENT.lexically_relative(ROOT);
        if (RELATIVE.empty() || RELATIVE.is_absolute() || *RELATIVE.begin() == "..")
            return std::nullopt;

        return RESULT == ROOT ? std::nullopt : std::optional{RESULT};
    }

    Hyprutils::OS::CFileDescriptor openValidatedPluginBinary(const std::filesystem::path& source, uid_t owner) {
        // O_NOFOLLOW plus the fstat checks below bind installation to an already-open, owned regular file rather than a replaceable path.
        Hyprutils::OS::CFileDescriptor sourceFD{open(source.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)}; // flawfinder: ignore
        if (!sourceFD.isValid())
            return {};

        struct stat sourceStat;
        if (fstat(sourceFD.get(), &sourceStat) != 0 || !S_ISREG(sourceStat.st_mode) || sourceStat.st_uid != owner)
            return {};

        return sourceFD;
    }
}
