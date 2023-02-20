#pragma once

#include "../defines.hpp"
#include "PluginAPI.hpp"

class CPlugin {
  public:
    std::string                                            name        = "";
    std::string                                            description = "";
    std::string                                            author      = "";
    std::string                                            version     = "";

    std::string                                            path = "";

    HANDLE                                                 m_pHandle = nullptr;

    std::vector<std::pair<std::string, HOOK_CALLBACK_FN*>> registeredCallbacks;
};

class CPluginSystem {
  public:
    CPlugin*              loadPlugin(const std::string& path);
    void                  unloadPlugin(const CPlugin* plugin, bool eject = false);
    CPlugin*              getPluginByPath(const std::string& path);
    CPlugin*              getPluginByHandle(HANDLE handle);
    std::vector<CPlugin*> getAllPlugins();

  private:
    std::vector<std::unique_ptr<CPlugin>> m_vLoadedPlugins;
};

inline std::unique_ptr<CPluginSystem> g_pPluginSystem;