#include "RepositoryCache.hpp"

#include <bit>
#include <system_error>
#include <sys/stat.h>

uint64_t NRepositoryCache::key(std::string_view url) {
    uint64_t hash = 14695981039346656037ULL;

    for (const auto c : url) {
        hash ^= std::bit_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }

    return hash;
}

bool NRepositoryCache::secureDirectory(const std::filesystem::path& path, uid_t owner) {
    struct stat statBuf;
    if (lstat(path.c_str(), &statBuf) != 0)
        return false;

    return S_ISDIR(statBuf.st_mode) && statBuf.st_uid == owner && !(statBuf.st_mode & (S_IWGRP | S_IWOTH));
}

std::optional<std::filesystem::path> NRepositoryCache::resolvePathSlotWithin(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    if (candidate.empty() || candidate.is_absolute() || candidate.filename().empty())
        return std::nullopt;

    std::error_code ec;
    const auto      CANONICAL_ROOT = std::filesystem::canonical(root, ec);
    if (ec)
        return std::nullopt;

    const auto RESOLVED_PARENT = std::filesystem::weakly_canonical(CANONICAL_ROOT / candidate.parent_path(), ec);
    if (ec)
        return std::nullopt;

    const auto RELATIVE_PARENT = RESOLVED_PARENT.lexically_relative(CANONICAL_ROOT);
    if (RELATIVE_PARENT.is_absolute() || (!RELATIVE_PARENT.empty() && *RELATIVE_PARENT.begin() == ".."))
        return std::nullopt;

    const auto SLOT = (RESOLVED_PARENT / candidate.filename()).lexically_normal();
    if (SLOT == CANONICAL_ROOT)
        return std::nullopt;

    return SLOT;
}

std::optional<std::filesystem::path> NRepositoryCache::resolvePathWithin(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    std::error_code ec;
    const auto      CANONICAL_ROOT = std::filesystem::canonical(root, ec);
    if (ec)
        return std::nullopt;

    const auto RESOLVED = std::filesystem::weakly_canonical(CANONICAL_ROOT / candidate, ec);
    if (ec || RESOLVED == CANONICAL_ROOT)
        return std::nullopt;

    const auto RELATIVE = RESOLVED.lexically_relative(CANONICAL_ROOT);
    if (RELATIVE.empty() || RELATIVE.is_absolute() || *RELATIVE.begin() == "..")
        return std::nullopt;

    return RESOLVED;
}

bool NRepositoryCache::regularFileOwnedBy(const std::filesystem::path& path, uid_t owner) {
    struct stat statBuf;
    return lstat(path.c_str(), &statBuf) == 0 && S_ISREG(statBuf.st_mode) && statBuf.st_uid == owner;
}
