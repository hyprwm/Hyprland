#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <sys/types.h>

namespace NRepositoryCache {
    uint64_t                             key(std::string_view url);
    bool                                 secureDirectory(const std::filesystem::path& path, uid_t owner);
    std::optional<std::filesystem::path> resolvePathSlotWithin(const std::filesystem::path& root, const std::filesystem::path& candidate);
    std::optional<std::filesystem::path> resolvePathWithin(const std::filesystem::path& root, const std::filesystem::path& candidate);
    bool                                 regularFileOwnedBy(const std::filesystem::path& path, uid_t owner);
}
