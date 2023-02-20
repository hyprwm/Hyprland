#define WLR_USE_UNSTABLE

#include "../../src/plugins/PluginAPI.hpp"
#include "../../src/Window.hpp"
#include "customLayout.hpp"

#include <unistd.h>
#include <thread>

inline HANDLE                             PHANDLE = nullptr;
inline std::unique_ptr<CHyprCustomLayout> g_pCustomLayout;

// Methods

static void onActiveWindowChange(void* self, std::any data) {
    try {
        auto* const PWINDOW = std::any_cast<CWindow*>(data);

        HyprlandAPI::invokeHyprctlCommand("seterror", "rgba(6666eeff)    [ExamplePlugin] Active window: " + (PWINDOW ? PWINDOW->m_szTitle : "None"));
    } catch (std::bad_any_cast& e) { HyprlandAPI::invokeHyprctlCommand("seterror", "rgba(6666eeff)    [ExamplePlugin] Active window: None"); }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::invokeHyprctlCommand("seterror", "rgba(66ee66ff) Hello World from a Hyprland plugin!");

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "activeWindow", [&](void* self, std::any data) { onActiveWindowChange(self, data); });

    g_pCustomLayout = std::make_unique<CHyprCustomLayout>();

    HyprlandAPI::addLayout(PHANDLE, "custom", g_pCustomLayout.get());

    return {"ExamplePlugin", "An example plugin", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::invokeHyprctlCommand("seterror", "rgba(ee6666ff) Goodbye World from a Hyprland plugin!");
}