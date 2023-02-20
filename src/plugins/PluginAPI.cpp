#include "PluginAPI.hpp"
#include "../Compositor.hpp"
#include "../debug/HyprCtl.hpp"

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
    std::string COMMAND = format + "/" + call + " " + args;
    return HyprCtl::makeDynamicCall(COMMAND);
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