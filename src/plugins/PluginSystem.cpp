#include "PluginSystem.hpp"

#include <dlfcn.h>
#include <ranges>
#include "../Compositor.hpp"

CPluginSystem::CPluginSystem() {
    g_pFunctionHookSystem = std::make_unique<CHookSystem>();
}

CPlugin* CPluginSystem::loadPlugin(const std::string& path) {

    m_szLastError = "";

    if (getPluginByPath(path)) {
        m_szLastError = "Cannot load a plugin twice!";
        Debug::log(ERR, " [PluginSystem] Cannot load a plugin twice!");
        return nullptr;
    }

    auto* const PLUGIN = m_vLoadedPlugins.emplace_back(std::make_unique<CPlugin>()).get();

    PLUGIN->path = path;

    HANDLE MODULE = dlopen(path.c_str(), RTLD_LAZY);

    if (!MODULE) {
        std::string strerr = dlerror();
        m_szLastError      = std::format("Plugin {} could not be loaded: {}", path, strerr);
        Debug::log(ERR, " [PluginSystem] Plugin {} could not be loaded: {}", path, strerr);
        m_vLoadedPlugins.pop_back();
        return nullptr;
    }

    PLUGIN->m_pHandle = MODULE;

    PPLUGIN_API_VERSION_FUNC apiVerFunc = (PPLUGIN_API_VERSION_FUNC)dlsym(MODULE, PLUGIN_API_VERSION_FUNC_STR);
    PPLUGIN_INIT_FUNC        initFunc   = (PPLUGIN_INIT_FUNC)dlsym(MODULE, PLUGIN_INIT_FUNC_STR);

    if (!apiVerFunc || !initFunc) {
        m_szLastError = std::format("Plugin {} could not be loaded: {}", path, "missing apiver/init func");
        Debug::log(ERR, " [PluginSystem] Plugin {} could not be loaded. (No apiver/init func)", path);
        dlclose(MODULE);
        m_vLoadedPlugins.pop_back();
        return nullptr;
    }

    const std::string PLUGINAPIVER = apiVerFunc();

    if (PLUGINAPIVER != HYPRLAND_API_VERSION) {
        m_szLastError = std::format("Plugin {} could not be loaded: {}", path, "API version mismatch");
        Debug::log(ERR, " [PluginSystem] Plugin {} could not be loaded. (API version mismatch)", path);
        dlclose(MODULE);
        m_vLoadedPlugins.pop_back();
        return nullptr;
    }

    PLUGIN_DESCRIPTION_INFO PLUGINDATA;

    try {
        if (!setjmp(m_jbPluginFaultJumpBuf)) {
            m_bAllowConfigVars = true;
            PLUGINDATA         = initFunc(MODULE);
        } else {
            // this module crashed.
            throw std::runtime_error("received a fatal signal");
        }
    } catch (std::exception& e) {
        m_bAllowConfigVars = false;
        m_szLastError      = std::format("Plugin {} could not be loaded: plugin crashed/threw in main: {}", path, e.what());
        Debug::log(ERR, " [PluginSystem] Plugin {} (Handle {:x}) crashed in init. Unloading.", path, (uintptr_t)MODULE);
        unloadPlugin(PLUGIN, true); // Plugin could've already hooked/done something
        return nullptr;
    }

    m_bAllowConfigVars = false;

    PLUGIN->author      = PLUGINDATA.author;
    PLUGIN->description = PLUGINDATA.description;
    PLUGIN->version     = PLUGINDATA.version;
    PLUGIN->name        = PLUGINDATA.name;

    g_pConfigManager->m_bForceReload = true;

    Debug::log(LOG, R"( [PluginSystem] Plugin {} loaded. Handle: {:x}, path: "{}", author: "{}", description: "{}", version: "{}")", PLUGINDATA.name, (uintptr_t)MODULE, path,
               PLUGINDATA.author, PLUGINDATA.description, PLUGINDATA.version);

    return PLUGIN;
}

void CPluginSystem::unloadPlugin(const CPlugin* plugin, bool eject) {
    if (!plugin)
        return;

    if (!eject) {
        PPLUGIN_EXIT_FUNC exitFunc = (PPLUGIN_EXIT_FUNC)dlsym(plugin->m_pHandle, PLUGIN_EXIT_FUNC_STR);
        if (exitFunc)
            exitFunc();
    }

    for (auto const& [k, v] : plugin->registeredCallbacks) {
        if (const auto SHP = v.lock())
            g_pHookSystem->unhook(SHP);
    }

    const auto ls = plugin->registeredLayouts;
    for (auto const& l : ls)
        g_pLayoutManager->removeLayout(l);

    g_pFunctionHookSystem->removeAllHooksFrom(plugin->m_pHandle);

    const auto rd = plugin->registeredDecorations;
    for (auto const& d : rd)
        HyprlandAPI::removeWindowDecoration(plugin->m_pHandle, d);

    const auto rdi = plugin->registeredDispatchers;
    for (auto const& d : rdi)
        HyprlandAPI::removeDispatcher(plugin->m_pHandle, d);

    const auto rhc = plugin->registeredHyprctlCommands;
    for (auto const& c : rhc)
        HyprlandAPI::unregisterHyprCtlCommand(plugin->m_pHandle, c);

    g_pConfigManager->removePluginConfig(plugin->m_pHandle);

    // save these two for dlclose and a log,
    // as erase_if will kill the pointer
    const auto PLNAME   = plugin->name;
    const auto PLHANDLE = plugin->m_pHandle;

    std::erase_if(m_vLoadedPlugins, [&](const auto& other) { return other->m_pHandle == PLHANDLE; });

    dlclose(PLHANDLE);

    Debug::log(LOG, " [PluginSystem] Plugin {} unloaded.", PLNAME);

    // reload config to fix some stuf like e.g. unloadedPluginVars
    g_pConfigManager->m_bForceReload = true;
}

void CPluginSystem::unloadAllPlugins() {
    for (auto const& p : m_vLoadedPlugins | std::views::reverse)
        unloadPlugin(p.get(), false); // Unload remaining plugins gracefully
}

std::vector<std::string> CPluginSystem::updateConfigPlugins(const std::vector<std::string>& plugins, bool& changed) {
    std::vector<std::string> failures;

    // unload all plugins that are no longer present
    for (auto const& p : m_vLoadedPlugins | std::views::reverse) {
        if (p->m_bLoadedWithConfig && std::find(plugins.begin(), plugins.end(), p->path) == plugins.end()) {
            Debug::log(LOG, "Unloading plugin {} which is no longer present in config", p->path);
            unloadPlugin(p.get(), false);
            changed = true;
        }
    }

    // load all new plugins
    for (auto const& path : plugins) {
        if (std::find_if(m_vLoadedPlugins.begin(), m_vLoadedPlugins.end(), [&](const auto& other) { return other->path == path; }) == m_vLoadedPlugins.end()) {
            Debug::log(LOG, "Loading plugin {} which is now present in config", path);
            const auto plugin = loadPlugin(path);

            if (plugin) {
                plugin->m_bLoadedWithConfig = true;
                changed                     = true;
            } else
                failures.push_back(path);
        }
    }

    return failures;
}

CPlugin* CPluginSystem::getPluginByPath(const std::string& path) {
    for (auto const& p : m_vLoadedPlugins) {
        if (p->path == path)
            return p.get();
    }

    return nullptr;
}

CPlugin* CPluginSystem::getPluginByHandle(HANDLE handle) {
    for (auto const& p : m_vLoadedPlugins) {
        if (p->m_pHandle == handle)
            return p.get();
    }

    return nullptr;
}

std::vector<CPlugin*> CPluginSystem::getAllPlugins() {
    std::vector<CPlugin*> results(m_vLoadedPlugins.size());
    for (size_t i = 0; i < m_vLoadedPlugins.size(); ++i)
        results[i] = m_vLoadedPlugins[i].get();
    return results;
}

size_t CPluginSystem::pluginCount() {
    return m_vLoadedPlugins.size();
}

void CPluginSystem::sigGetPlugins(CPlugin** data, size_t len) {
    for (size_t i = 0; i < std::min(m_vLoadedPlugins.size(), len); i++) {
        data[i] = m_vLoadedPlugins[i].get();
    }
}
