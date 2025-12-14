#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "Plugin.hpp"

struct SGlobalState {
    std::string headersAbiCompiled = "";
    bool        dontWarnInstall    = false;
};

namespace DataState {
    std::filesystem::path              getDataStatePath();
    std::string                        getHeadersPath();
    std::vector<std::filesystem::path> getPluginStates();
    void                               ensureStateStoreExists();
    void                               addNewPluginRepo(const SPluginRepository& repo);
    void                               removePluginRepo(const SPluginRepoIdentifier identifier);
    bool                               pluginRepoExists(const SPluginRepoIdentifier identifier);
    void                               updateGlobalState(const SGlobalState& state);
    void                               purgeAllCache();
    SGlobalState                       getGlobalState();
    bool                               setPluginEnabled(const SPluginRepoIdentifier identifier, bool enabled);
    std::vector<SPluginRepository>     getAllRepositories();
};
