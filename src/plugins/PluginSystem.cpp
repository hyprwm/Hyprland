#include "PluginSystem.hpp"

#include <dlfcn.h>
#include <ranges>
#include "../config/ConfigManager.hpp"
#include "../debug/HyprCtl.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/permissions/DynamicPermissionManager.hpp"
#include "../debug/HyprNotificationOverlay.hpp"

CPluginSystem::CPluginSystem() {
    g_pFunctionHookSystem = makeUnique<CHookSystem>();
}

SP<CPromise<CPlugin*>> CPluginSystem::loadPlugin(const std::string& path, eSpecialPidTypes pidType) {

    pid_t pid = 0;

    if (g_pHyprCtl->m_currentRequestParams.pid > 0)
        pid = g_pHyprCtl->m_currentRequestParams.pid;

    return CPromise<CPlugin*>::make([path, pid, pidType, this](SP<CPromiseResolver<CPlugin*>> resolver) {
        const auto PERM = g_pDynamicPermissionManager->clientPermissionModeWithString(pidType != SPECIAL_PID_TYPE_NONE ? pidType : pid, path, PERMISSION_TYPE_PLUGIN);
        if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING) {
            Debug::log(LOG, "CPluginSystem: Waiting for user confirmation to load {}", path);

            auto promise = g_pDynamicPermissionManager->promiseFor(pid, path, PERMISSION_TYPE_PLUGIN);
            if (!promise) { // already awaiting or something?
                resolver->reject("Failed to get a promise for permission");
                return;
            }

            promise->then([this, path, resolver](SP<CPromiseResult<eDynamicPermissionAllowMode>> result) {
                if (result->hasError()) {
                    Debug::log(ERR, "CPluginSystem: Error spawning permission prompt");
                    resolver->reject("Error spawning permission prompt");
                    return;
                }

                if (result->result() != PERMISSION_RULE_ALLOW_MODE_ALLOW) {
                    Debug::log(ERR, "CPluginSystem: Rejecting plugin load of {}, user denied", path);
                    resolver->reject("user denied");
                    return;
                }

                Debug::log(LOG, "CPluginSystem: Loading {}, user allowed", path);

                const auto RESULT = loadPluginInternal(path);
                if (RESULT.has_value())
                    resolver->resolve(RESULT.value());
                else
                    resolver->reject(RESULT.error());
            });
            return;
        } else if (PERM == PERMISSION_RULE_ALLOW_MODE_DENY) {
            Debug::log(LOG, "CPluginSystem: Rejecting plugin load, permission is disabled");
            resolver->reject("permission is disabled");
            return;
        }

        const auto RESULT = loadPluginInternal(path);
        if (RESULT.has_value())
            resolver->resolve(RESULT.value());
        else
            resolver->reject(RESULT.error());
    });
}

std::expected<CPlugin*, std::string> CPluginSystem::loadPluginInternal(const std::string& path) {
    if (getPluginByPath(path)) {
        Debug::log(ERR, " [PluginSystem] Cannot load a plugin twice!");
        return std::unexpected("Cannot load a plugin twice!");
    }

    auto* const PLUGIN = m_loadedPlugins.emplace_back(makeUnique<CPlugin>()).get();

    PLUGIN->m_path = path;

    HANDLE MODULE = dlopen(path.c_str(), RTLD_LAZY);

    if (!MODULE) {
        std::string strerr = dlerror();
        Debug::log(ERR, " [PluginSystem] Plugin {} could not be loaded: {}", path, strerr);
        m_loadedPlugins.pop_back();
        return std::unexpected(std::format("Plugin {} could not be loaded: {}", path, strerr));
    }

    PLUGIN->m_handle = MODULE;

    PPLUGIN_API_VERSION_FUNC apiVerFunc = static_cast<PPLUGIN_API_VERSION_FUNC>(dlsym(MODULE, PLUGIN_API_VERSION_FUNC_STR));
    PPLUGIN_INIT_FUNC        initFunc   = static_cast<PPLUGIN_INIT_FUNC>(dlsym(MODULE, PLUGIN_INIT_FUNC_STR));

    if (!apiVerFunc || !initFunc) {
        Debug::log(ERR, " [PluginSystem] Plugin {} could not be loaded. (No apiver/init func)", path);
        dlclose(MODULE);
        m_loadedPlugins.pop_back();
        return std::unexpected(std::format("Plugin {} could not be loaded: {}", path, "missing apiver/init func"));
    }

    const std::string PLUGINAPIVER = apiVerFunc();

    if (PLUGINAPIVER != HYPRLAND_API_VERSION) {
        Debug::log(ERR, " [PluginSystem] Plugin {} could not be loaded. (API version mismatch)", path);
        dlclose(MODULE);
        m_loadedPlugins.pop_back();
        return std::unexpected(std::format("Plugin {} could not be loaded: {}", path, "API version mismatch"));
    }

    PLUGIN_DESCRIPTION_INFO PLUGINDATA;

    try {
        if (!setjmp(m_pluginFaultJumpBuf)) {
            m_allowConfigVars = true;
            PLUGINDATA        = initFunc(MODULE);
        } else {
            // this module crashed.
            throw std::runtime_error("received a fatal signal");
        }
    } catch (std::exception& e) {
        m_allowConfigVars = false;
        Debug::log(ERR, " [PluginSystem] Plugin {} (Handle {:x}) crashed in init. Unloading.", path, reinterpret_cast<uintptr_t>(MODULE));
        unloadPlugin(PLUGIN, true); // Plugin could've already hooked/done something
        return std::unexpected(std::format("Plugin {} could not be loaded: plugin crashed/threw in main: {}", path, e.what()));
    }

    m_allowConfigVars = false;

    PLUGIN->m_author      = PLUGINDATA.author;
    PLUGIN->m_description = PLUGINDATA.description;
    PLUGIN->m_version     = PLUGINDATA.version;
    PLUGIN->m_name        = PLUGINDATA.name;

    g_pEventLoopManager->doLater([] { g_pConfigManager->reload(); });

    Debug::log(LOG, R"( [PluginSystem] Plugin {} loaded. Handle: {:x}, path: "{}", author: "{}", description: "{}", version: "{}")", PLUGINDATA.name, reinterpret_cast<uintptr_t>(MODULE), path,
               PLUGINDATA.author, PLUGINDATA.description, PLUGINDATA.version);

    return PLUGIN;
}

void CPluginSystem::unloadPlugin(const CPlugin* plugin, bool eject) {
    if (!plugin)
        return;

    if (!eject) {
        PPLUGIN_EXIT_FUNC exitFunc = static_cast<PPLUGIN_EXIT_FUNC>(dlsym(plugin->m_handle, PLUGIN_EXIT_FUNC_STR));
        if (exitFunc)
            exitFunc();
    }

    for (auto const& [k, v] : plugin->m_registeredCallbacks) {
        if (const auto SHP = v.lock())
            g_pHookSystem->unhook(SHP);
    }

    const auto ls = plugin->m_registeredLayouts;
    for (auto const& l : ls)
        g_pLayoutManager->removeLayout(l);

    g_pFunctionHookSystem->removeAllHooksFrom(plugin->m_handle);

    const auto rd = plugin->m_registeredDecorations;
    for (auto const& d : rd)
        HyprlandAPI::removeWindowDecoration(plugin->m_handle, d);

    const auto rdi = plugin->m_registeredDispatchers;
    for (auto const& d : rdi)
        HyprlandAPI::removeDispatcher(plugin->m_handle, d);

    const auto rhc = plugin->m_registeredHyprctlCommands;
    for (auto const& c : rhc)
        HyprlandAPI::unregisterHyprCtlCommand(plugin->m_handle, c);

    g_pConfigManager->removePluginConfig(plugin->m_handle);

    // save these two for dlclose and a log,
    // as erase_if will kill the pointer
    const auto PLNAME   = plugin->m_name;
    const auto PLHANDLE = plugin->m_handle;

    std::erase_if(m_loadedPlugins, [&](const auto& other) { return other->m_handle == PLHANDLE; });

    dlclose(PLHANDLE);

    Debug::log(LOG, " [PluginSystem] Plugin {} unloaded.", PLNAME);

    // reload config to fix some stuf like e.g. unloadedPluginVars
    g_pEventLoopManager->doLater([] { g_pConfigManager->reload(); });
}

void CPluginSystem::unloadAllPlugins() {
    for (auto const& p : m_loadedPlugins | std::views::reverse)
        unloadPlugin(p.get(), false); // Unload remaining plugins gracefully
}

void CPluginSystem::updateConfigPlugins(const std::vector<std::string>& plugins, bool& changed) {
    if (m_lastConfigPlugins == plugins)
        return;

    m_lastConfigPlugins = plugins;

    // unload all plugins that are no longer present
    for (auto const& p : m_loadedPlugins | std::views::reverse) {
        if (!p->m_loadedWithConfig || std::ranges::find(plugins, p->m_path) != plugins.end())
            continue;

        Debug::log(LOG, "Unloading plugin {} which is no longer present in config", p->m_path);
        unloadPlugin(p.get(), false);
        changed = true;
    }

    // load all new plugins
    for (auto const& path : plugins) {
        if (std::ranges::find_if(m_loadedPlugins, [&](const auto& other) { return other->m_path == path; }) != m_loadedPlugins.end())
            continue;

        Debug::log(LOG, "Loading plugin {} which is now present in config", path);

        changed = true;

        loadPlugin(path, SPECIAL_PID_TYPE_CONFIG)->then([path](SP<CPromiseResult<CPlugin*>> result) {
            if (result->hasError()) {
                const auto NAME = path.contains('/') ? path.substr(path.find_last_of('/') + 1) : path;
                Debug::log(ERR, "CPluginSystem::updateConfigPlugins: failed to load plugin {}: {}", NAME, result->error());
                g_pHyprNotificationOverlay->addNotification(std::format("Failed to load plugin {}: {}", NAME, result->error()), CHyprColor{0, 0, 0, 0}, 5000, ICON_ERROR);
                return;
            }

            result->result()->m_loadedWithConfig = true;

            Debug::log(LOG, "CPluginSystem::updateConfigPlugins: loaded {}", path);
        });
    }
}

CPlugin* CPluginSystem::getPluginByPath(const std::string& path) {
    for (auto const& p : m_loadedPlugins) {
        if (p->m_path == path)
            return p.get();
    }

    return nullptr;
}

CPlugin* CPluginSystem::getPluginByHandle(HANDLE handle) {
    for (auto const& p : m_loadedPlugins) {
        if (p->m_handle == handle)
            return p.get();
    }

    return nullptr;
}

std::vector<CPlugin*> CPluginSystem::getAllPlugins() {
    std::vector<CPlugin*> results(m_loadedPlugins.size());
    for (size_t i = 0; i < m_loadedPlugins.size(); ++i)
        results[i] = m_loadedPlugins[i].get();
    return results;
}

size_t CPluginSystem::pluginCount() {
    return m_loadedPlugins.size();
}

void CPluginSystem::sigGetPlugins(CPlugin** data, size_t len) {
    for (size_t i = 0; i < std::min(m_loadedPlugins.size(), len); i++) {
        data[i] = m_loadedPlugins[i].get();
    }
}
