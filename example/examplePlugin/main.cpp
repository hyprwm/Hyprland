#define WLR_USE_UNSTABLE

#include "globals.hpp"

#include "../../src/Window.hpp"
#include "../../src/Compositor.hpp"
#include "customLayout.hpp"
#include "customDecoration.hpp"

#include <unistd.h>
#include <thread>

// Methods
inline std::unique_ptr<CHyprCustomLayout> g_pCustomLayout;
inline CFunctionHook*                     g_pFocusHook = nullptr;
typedef void (*origFocusWindow)(void*, CWindow*, wlr_surface*);

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void onActiveWindowChange(void* self, std::any data) {
    try {
        auto* const PWINDOW = std::any_cast<CWindow*>(data);

        HyprlandAPI::addNotification(PHANDLE, "[ExamplePlugin] Active window: " + (PWINDOW ? PWINDOW->m_szTitle : "None"), CColor{0.f, 0.5f, 1.f, 1.f}, 5000);
    } catch (std::bad_any_cast& e) { HyprlandAPI::addNotification(PHANDLE, "[ExamplePlugin] Active window: None", CColor{0.f, 0.5f, 1.f, 1.f}, 5000); }
}

static void onNewWindow(void* self, std::any data) {
    auto* const PWINDOW = std::any_cast<CWindow*>(data);

    HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, new CCustomDecoration(PWINDOW));
}

void hkFocusWindow(void* thisptr, CWindow* pWindow, wlr_surface* pSurface) {

    // stuf

    (*(origFocusWindow)g_pFocusHook->m_pOriginal)(thisptr, pWindow, pSurface);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addNotification(PHANDLE, "Hello World from an example plugin!", CColor{0.f, 1.f, 1.f, 1.f}, 5000);

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "activeWindow", [&](void* self, std::any data) { onActiveWindowChange(self, data); });
    HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, std::any data) { onNewWindow(self, data); });

    g_pCustomLayout = std::make_unique<CHyprCustomLayout>();

    HyprlandAPI::addLayout(PHANDLE, "custom", g_pCustomLayout.get());

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:example:border_color", SConfigValue{.intValue = configStringToInt("rgb(44ee44)")});

    g_pFocusHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CCompositor::focusWindow, (void*)&hkFocusWindow);
    g_pFocusHook->hook();

    HyprlandAPI::reloadConfig();

    return {"ExamplePlugin", "An example plugin", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::invokeHyprctlCommand("seterror", "disable");
}