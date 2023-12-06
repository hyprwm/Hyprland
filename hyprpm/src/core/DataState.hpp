#pragma once
#include <string>
#include <vector>
#include "Plugin.hpp"

struct SGlobalState {
    std::string headersHashCompiled;
};

namespace DataState {
    void                           addNewPluginRepo(const SPluginRepository& repo);
    void                           removePluginRepo(const std::string& urlOrName);
    bool                           pluginRepoExists(const std::string& urlOrName);
    void                           updateGlobalState(const SGlobalState& state);
    bool                           setPluginEnabled(const std::string& name, bool enabled);
    std::vector<SPluginRepository> getAllRepositories();
};