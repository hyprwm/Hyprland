#define WLR_USE_UNSTABLE

#include "../../src/plugins/PluginAPI.hpp"
#include "../../src/Window.hpp"

#include <unistd.h>
#include <thread>

inline HANDLE PHANDLE = nullptr;

// Methods

static void onActiveWindowChange(void* self, std::any data) {
    auto* const PWINDOW = std::any_cast<CWindow*>(data);

    std::thread(
        [&](CWindow* pWindow) {
            std::string text = "hyprctl seterror \"rgba(6666eeff)\" \"   [ExamplePlugin] Active window: \"" + (pWindow ? pWindow->m_szTitle : "");
            system(text.c_str());
        },
        PWINDOW)
        .detach();
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO pluginInit(HANDLE handle) {
    PHANDLE = handle;

    std::thread([&]() { system("hyprctl seterror \"rgba(66ee66ff)\" Hello World from a Hyprland plugin!"); }).detach();

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "activeWindow", [&](void* self, std::any data) { onActiveWindowChange(self, data); });

    return {"ExamplePlugin", "An example plugin", "Vaxry", "1.0"};
}

APICALL EXPORT void pluginExit() {
    std::thread([&]() { system("hyprctl seterror \"rgba(ee6666ff)\" Bye World from a Hyprland plugin!"); }).detach();
}