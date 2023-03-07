#include "PluginAPI.hpp"
#include "../Compositor.hpp"
#include "../debug/HyprCtl.hpp"
#include <dlfcn.h>

APICALL bool HyprlandAPI::registerCallbackStatic(HANDLE handle, const std::string& event, HOOK_CALLBACK_FN* fn) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    g_pHookSystem->hookStatic(event, fn, handle);
    PLUGIN->registeredCallbacks.emplace_back(std::make_pair<>(event, fn));

    return true;
}

APICALL HOOK_CALLBACK_FN* HyprlandAPI::registerCallbackDynamic(HANDLE handle, const std::string& event, HOOK_CALLBACK_FN fn) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return nullptr;

    auto* const PFN = g_pHookSystem->hookDynamic(event, fn, handle);
    PLUGIN->registeredCallbacks.emplace_back(std::make_pair<>(event, PFN));
    return PFN;
}

APICALL bool HyprlandAPI::unregisterCallback(HANDLE handle, HOOK_CALLBACK_FN* fn) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    g_pHookSystem->unhook(fn);
    std::erase_if(PLUGIN->registeredCallbacks, [&](const auto& other) { return other.second == fn; });

    return true;
}

APICALL std::string HyprlandAPI::invokeHyprctlCommand(const std::string& call, const std::string& args, const std::string& format) {
    if (args.empty())
        return HyprCtl::makeDynamicCall(format + "/" + call);
    else
        return HyprCtl::makeDynamicCall(format + "/" + call + " " + args);
}

APICALL bool HyprlandAPI::addLayout(HANDLE handle, const std::string& name, IHyprLayout* layout) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    PLUGIN->registeredLayouts.push_back(layout);

    return g_pLayoutManager->addLayout(name, layout);
}

APICALL bool HyprlandAPI::removeLayout(HANDLE handle, IHyprLayout* layout) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    std::erase(PLUGIN->registeredLayouts, layout);

    return g_pLayoutManager->removeLayout(layout);
}

APICALL bool HyprlandAPI::reloadConfig() {
    g_pConfigManager->m_bForceReload = true;
    return true;
}

APICALL bool HyprlandAPI::addNotification(HANDLE handle, const std::string& text, const CColor& color, const float timeMs) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    g_pHyprNotificationOverlay->addNotification(text, color, timeMs);

    return true;
}

APICALL CFunctionHook* HyprlandAPI::createFunctionHook(HANDLE handle, const void* source, const void* destination) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return nullptr;

    return g_pFunctionHookSystem->initHook(handle, (void*)source, (void*)destination);
}

APICALL bool HyprlandAPI::removeFunctionHook(HANDLE handle, CFunctionHook* hook) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    return g_pFunctionHookSystem->removeHook(hook);
}

APICALL bool HyprlandAPI::addWindowDecoration(HANDLE handle, CWindow* pWindow, IHyprWindowDecoration* pDecoration) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    if (!g_pCompositor->windowValidMapped(pWindow))
        return false;

    PLUGIN->registeredDecorations.push_back(pDecoration);

    pWindow->m_dWindowDecorations.emplace_back(pDecoration);

    return true;
}

APICALL bool HyprlandAPI::removeWindowDecoration(HANDLE handle, IHyprWindowDecoration* pDecoration) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    for (auto& w : g_pCompositor->m_vWindows) {
        for (auto& d : w->m_dWindowDecorations) {
            if (d.get() == pDecoration) {
                std::erase(w->m_dWindowDecorations, d);
                return true;
            }
        }
    }

    return false;
}

APICALL bool HyprlandAPI::addConfigValue(HANDLE handle, const std::string& name, const SConfigValue& value) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!g_pPluginSystem->m_bAllowConfigVars)
        return false;

    if (!PLUGIN)
        return false;

    if (name.find("plugin:") != 0)
        return false;

    g_pConfigManager->addPluginConfigVar(handle, name, value);
    return true;
}

APICALL SConfigValue* HyprlandAPI::getConfigValue(HANDLE handle, const std::string& name) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return nullptr;

    return g_pConfigManager->getConfigValuePtrSafe(name);
}

APICALL void* HyprlandAPI::getFunctionAddressFromSignature(HANDLE handle, const std::string& sig) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return nullptr;

    return dlsym(nullptr, sig.c_str());
}

APICALL bool HyprlandAPI::addDispatcher(HANDLE handle, const std::string& name, std::function<void(std::string)> handler) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    PLUGIN->registeredDispatchers.push_back(name);

    g_pKeybindManager->m_mDispatchers[name] = handler;

    return true;
}

APICALL bool HyprlandAPI::removeDispatcher(HANDLE handle, const std::string& name) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    std::erase_if(g_pKeybindManager->m_mDispatchers, [&](const auto& other) { return other.first == name; });
    std::erase_if(PLUGIN->registeredDispatchers, [&](const auto& other) { return other == name; });

    return true;
}
