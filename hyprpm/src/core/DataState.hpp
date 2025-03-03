#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "Plugin.hpp"

struct SGlobalState {
    std::string headersHashCompiled = "";
    bool        dontWarnInstall     = false;
};

namespace NDataState {
    static std::filesystem::path              getDataStatePath();
    static std::string                        getHeadersPath();
    static std::vector<std::filesystem::path> getPluginStates();
    static void                               ensureStateStoreExists();
    static void                               addNewPluginRepo(const SPluginRepository& repo);
    static void                               removePluginRepo(const std::string& urlOrName);
    static bool                               pluginRepoExists(const std::string& urlOrName);
    static void                               updateGlobalState(const SGlobalState& state);
    static SGlobalState                       getGlobalState();
    static bool                               setPluginEnabled(const std::string& name, bool enabled);
    static std::vector<SPluginRepository>     getAllRepositories();
};