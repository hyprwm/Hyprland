#pragma once

#include "../defines.hpp"
#include "../helpers/defer/Promise.hpp"
#include "PluginAPI.hpp"
#include <csetjmp>
#include <expected>

class IHyprWindowDecoration;

class CPlugin {
  public:
    std::string                                               m_name        = "";
    std::string                                               m_description = "";
    std::string                                               m_author      = "";
    std::string                                               m_version     = "";

    std::string                                               m_path = "";

    bool                                                      m_loadedWithConfig = false;

    HANDLE                                                    m_handle = nullptr;

    std::vector<IHyprLayout*>                                 m_registeredLayouts;
    std::vector<IHyprWindowDecoration*>                       m_registeredDecorations;
    std::vector<std::pair<std::string, WP<HOOK_CALLBACK_FN>>> m_registeredCallbacks;
    std::vector<std::string>                                  m_registeredDispatchers;
    std::vector<SP<SHyprCtlCommand>>                          m_registeredHyprctlCommands;
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

    bool                   m_allowConfigVars = false;

  private:
    std::vector<UP<CPlugin>>             m_loadedPlugins;

    jmp_buf                              m_pluginFaultJumpBuf;

    std::expected<CPlugin*, std::string> loadPluginInternal(const std::string& path);
};

inline UP<CPluginSystem> g_pPluginSystem;
