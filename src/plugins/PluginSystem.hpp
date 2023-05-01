#pragma once

#include "../defines.hpp"
#include "PluginAPI.hpp"
#include <csetjmp>

class IHyprWindowDecoration;

class CPlugin {
  public:
    std::string                                            name        = "";
    std::string                                            description = "";
    std::string                                            author      = "";
    std::string                                            version     = "";

    std::string                                            path = "";

    bool                                                   m_bLoadedWithConfig = false;

    HANDLE                                                 m_pHandle = nullptr;

    std::vector<IHyprLayout*>                              registeredLayouts;
    std::vector<IHyprWindowDecoration*>                    registeredDecorations;
    std::vector<std::pair<std::string, HOOK_CALLBACK_FN*>> registeredCallbacks;
    std::vector<std::string>                               registeredDispatchers;
};

class CPluginSystem {
  public:
    CPluginSystem();

    CPlugin*                 loadPlugin(const std::string& path);
    void                     unloadPlugin(const CPlugin* plugin, bool eject = false);
    void                     unloadAllPlugins();
    std::vector<std::string> updateConfigPlugins(const std::vector<std::string>& plugins, bool& changed);
    CPlugin*                 getPluginByPath(const std::string& path);
    CPlugin*                 getPluginByHandle(HANDLE handle);
    std::vector<CPlugin*>    getAllPlugins();

    bool                     m_bAllowConfigVars = false;

  private:
    std::vector<std::unique_ptr<CPlugin>> m_vLoadedPlugins;

    jmp_buf                               m_jbPluginFaultJumpBuf;
};

inline std::unique_ptr<CPluginSystem> g_pPluginSystem;
