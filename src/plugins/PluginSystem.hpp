#pragma once

#include "../defines.hpp"
#include "../helpers/defer/Promise.hpp"
#include "PluginAPI.hpp"
#include <csetjmp>
#include <expected>

class IHyprWindowDecoration;

class CPlugin {
  public:
    std::string                                               name        = "";
    std::string                                               description = "";
    std::string                                               author      = "";
    std::string                                               version     = "";

    std::string                                               path = "";

    bool                                                      m_bLoadedWithConfig = false;

    HANDLE                                                    m_pHandle = nullptr;

    std::vector<IHyprLayout*>                                 registeredLayouts;
    std::vector<IHyprWindowDecoration*>                       registeredDecorations;
    std::vector<std::pair<std::string, WP<HOOK_CALLBACK_FN>>> registeredCallbacks;
    std::vector<std::string>                                  registeredDispatchers;
    std::vector<SP<SHyprCtlCommand>>                          registeredHyprctlCommands;
};

class CPluginSystem {
  public:
    CPluginSystem();

    SP<CPromise<CPlugin*>> loadPlugin(const std::string& path);
    void                   unloadPlugin(const CPlugin* plugin, bool eject = false);
    void                   unloadAllPlugins();
    void                   updateConfigPlugins(const std::vector<std::string>& plugins, bool& changed);
    CPlugin*               getPluginByPath(const std::string& path);
    CPlugin*               getPluginByHandle(HANDLE handle);
    std::vector<CPlugin*>  getAllPlugins();
    size_t                 pluginCount();
    void                   sigGetPlugins(CPlugin** data, size_t len);

    bool                   m_bAllowConfigVars = false;

  private:
    std::vector<UP<CPlugin>>             m_vLoadedPlugins;

    jmp_buf                              m_jbPluginFaultJumpBuf;

    std::expected<CPlugin*, std::string> loadPluginInternal(const std::string& path);
};

inline UP<CPluginSystem> g_pPluginSystem;
