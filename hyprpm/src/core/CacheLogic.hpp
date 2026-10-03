#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <sys/types.h>

#include <hyprutils/os/FileDescriptor.hpp>

namespace NCacheLogic {
    std::filesystem::path repositoryPath(const std::filesystem::path& cacheRoot, std::string_view url);
    std::filesystem::path workingRepositoryPath(bool experimentalCache, const std::filesystem::path& cacheRoot, const std::filesystem::path& tempRoot, std::string_view username,
                                                std::string_view url);
    bool                  repositoryNeedsUpdate(bool forceUpdate, std::string_view checkedOutHash, std::string_view storedHash);
    std::string           checkoutRepositoryRevisionCommand(const std::filesystem::path& repository, std::string_view revision);
    std::optional<std::filesystem::path> pluginOutputPath(const std::filesystem::path& repositoryRoot, std::string_view output);
    Hyprutils::OS::CFileDescriptor       openValidatedPluginBinary(const std::filesystem::path& source, uid_t owner);
}
