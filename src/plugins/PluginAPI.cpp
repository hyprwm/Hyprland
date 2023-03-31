#include "PluginAPI.hpp"
#include "../Compositor.hpp"
#include "../debug/HyprCtl.hpp"
#include <dlfcn.h>

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#endif

#include <sstream>

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

APICALL bool addNotificationV2(HANDLE handle, const std::unordered_map<std::string, std::any>& data) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return false;

    try {
        auto iterator = data.find("text");
        if (iterator == data.end())
            return false;

        // mandatory
        std::string text;
        try {
            text = std::any_cast<std::string>(iterator->second);
        } catch (std::exception& e) {
            // attempt const char*
            text = std::any_cast<const char*>(iterator->second);
        }

        iterator = data.find("time");
        if (iterator == data.end())
            return false;

        const auto TIME = std::any_cast<uint64_t>(iterator->second);

        iterator = data.find("color");
        if (iterator == data.end())
            return false;

        const auto COLOR = std::any_cast<CColor>(iterator->second);

        // optional
        eIcons icon = ICON_NONE;
        iterator    = data.find("icon");
        if (iterator != data.end())
            icon = std::any_cast<eIcons>(iterator->second);

        g_pHyprNotificationOverlay->addNotification(text, COLOR, TIME, icon);

    } catch (std::exception& e) {
        // bad any_cast most likely, plugin error
        return false;
    }

    return true;
}

APICALL std::vector<SFunctionMatch> HyprlandAPI::findFunctionsByName(HANDLE handle, const std::string& name) {
    auto* const PLUGIN = g_pPluginSystem->getPluginByHandle(handle);

    if (!PLUGIN)
        return std::vector<SFunctionMatch>{};

#if defined(KERN_PROC_PATHNAME)
    int mib[] = {
        CTL_KERN,
#if defined(__NetBSD__)
        KERN_PROC_ARGS,
        -1,
        KERN_PROC_PATHNAME,
#else
        KERN_PROC,
        KERN_PROC_PATHNAME,
        -1,
#endif
    };
    u_int  miblen        = sizeof(mib) / sizeof(mib[0]);
    char   exe[PATH_MAX] = "";
    size_t sz            = sizeof(exe);
    sysctl(mib, miblen, &exe, &sz, NULL, 0);
    const auto FPATH = std::filesystem::canonical(exe);
#elif defined(__OpenBSD__)
    // Neither KERN_PROC_PATHNAME nor /proc are supported
    const auto FPATH = std::filesystem::canonical("/usr/local/bin/Hyprland");
#else
    const auto FPATH = std::filesystem::canonical("/proc/self/exe");
#endif

#ifdef __clang__
    const auto SYMBOLS          = execAndGet(("llvm-nm -D -j " + FPATH.string()).c_str());
    const auto SYMBOLSDEMANGLED = execAndGet(("llvm-nm -D -j --demangle " + FPATH.string()).c_str());
#else
    const auto SYMBOLS          = execAndGet(("nm -D -j " + FPATH.string()).c_str());
    const auto SYMBOLSDEMANGLED = execAndGet(("nm -D -j --demangle=auto " + FPATH.string()).c_str());
#endif

    auto       demangledFromID = [&](size_t id) -> std::string {
        size_t pos   = 0;
        size_t count = 0;
        while (count < id) {
            pos++;
            pos = SYMBOLSDEMANGLED.find('\n', pos);
            if (pos == std::string::npos)
                return "";
            count++;
        }

        return SYMBOLSDEMANGLED.substr(pos, SYMBOLSDEMANGLED.find('\n', pos + 1) - pos);
    };

    std::vector<SFunctionMatch> matches;

    std::istringstream          inStream(SYMBOLS);
    std::string                 line;
    int                         lineNo = 0;
    while (std::getline(inStream, line)) {
        if (line.contains(name)) {
            void* address = dlsym(nullptr, line.c_str());

            if (!address)
                continue;

            matches.push_back({address, line, demangledFromID(lineNo)});
        }
        lineNo++;
    }

    return matches;
}