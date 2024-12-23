#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "Plugin.hpp"

struct SGlobalState {
    std::string headersHashCompiled = "";
    bool        dontWarnInstall     = false;
};

namespace DataState {
    std::filesystem::path              getDataStatePath();
    std::string                        getHeadersPath();
    std::vector<std::filesystem::path> getPluginStates();
    void                               ensureStateStoreExists();
    void                               addNewPluginRepo(const SPluginRepository& repo);
    void                               removePluginRepo(const std::string& urlOrName);
    bool                               pluginRepoExists(const std::string& urlOrName);
    void                               updateGlobalState(const SGlobalState& state);
    SGlobalState                       getGlobalState();
    bool                               setPluginEnabled(const std::string& name, bool enabled);
    std::vector<SPluginRepository>     getAllRepositories();
};