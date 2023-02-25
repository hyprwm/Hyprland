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
inline CFunctionHook*                     g_pFocusHook  = nullptr;
inline CFunctionHook*                     g_pMotionHook = nullptr;
typedef void (*origFocusWindow)(void*, CWindow*, wlr_surface*);
typedef void (*origMotion)(wlr_seat*, uint32_t, double, double);

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
    HyprlandAPI::addNotification(PHANDLE, getFormat("FocusWindow with %lx %lx", pWindow, pSurface), CColor{0.f, 1.f, 1.f, 1.f}, 5000);
    (*(origFocusWindow)g_pFocusHook->m_pOriginal)(thisptr, pWindow, pSurface);
}

void hkNotifyMotion(wlr_seat* wlr_seat, uint32_t time_msec, double sx, double sy) {
    HyprlandAPI::addNotification(PHANDLE, getFormat("NotifyMotion with %lf %lf", sx, sy), CColor{0.f, 1.f, 1.f, 1.f}, 5000);
    (*(origMotion)g_pMotionHook->m_pOriginal)(wlr_seat, time_msec, sx, sy);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addNotification(PHANDLE, "Hello World from an example plugin!", CColor{0.f, 1.f, 1.f, 1.f}, 5000);

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "activeWindow", [&](void* self, std::any data) { onActiveWindowChange(self, data); });
    HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, std::any data) { onNewWindow(self, data); });

    g_pCustomLayout = std::make_unique<CHyprCustomLayout>();

    HyprlandAPI::addLayout(PHANDLE, "custom", g_pCustomLayout.get());

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:example:border_color", SConfigValue{.intValue = configStringToInt("rgb(44ee44)")});

    g_pFocusHook  = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CCompositor::focusWindow, (void*)&hkFocusWindow);
    g_pMotionHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&wlr_seat_pointer_notify_motion, (void*)&hkNotifyMotion);
    g_pFocusHook->hook();
    g_pMotionHook->hook();

    HyprlandAPI::reloadConfig();

    return {"ExamplePlugin", "An example plugin", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::invokeHyprctlCommand("seterror", "disable");
}