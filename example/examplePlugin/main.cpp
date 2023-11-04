#define WLR_USE_UNSTABLE

#include "globals.hpp"

#include <hyprland/src/Window.hpp>
#include <hyprland/src/Compositor.hpp>
#include "customLayout.hpp"
#include "customDecoration.hpp"

#include <unistd.h>
#include <thread>

// Methods
inline std::unique_ptr<CHyprCustomLayout> g_pCustomLayout;
inline CFunctionHook*                     g_pFocusHook     = nullptr;
inline CFunctionHook*                     g_pMotionHook    = nullptr;
inline CFunctionHook*                     g_pMouseDownHook = nullptr;
typedef void (*origFocusWindow)(void*, CWindow*, wlr_surface*);
typedef void (*origMotion)(wlr_seat*, uint32_t, double, double);
typedef void (*origMouseDownNormal)(void*, wlr_pointer_button_event*);

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
    // HyprlandAPI::addNotification(PHANDLE, getFormat("FocusWindow with %lx %lx", pWindow, pSurface), CColor{0.f, 1.f, 1.f, 1.f}, 5000);
    (*(origFocusWindow)g_pFocusHook->m_pOriginal)(thisptr, pWindow, pSurface);
}

void hkNotifyMotion(wlr_seat* wlr_seat, uint32_t time_msec, double sx, double sy) {
    // HyprlandAPI::addNotification(PHANDLE, getFormat("NotifyMotion with %lf %lf", sx, sy), CColor{0.f, 1.f, 1.f, 1.f}, 5000);
    (*(origMotion)g_pMotionHook->m_pOriginal)(wlr_seat, time_msec, sx, sy);
}

void hkProcessMouseDownNormal(void* thisptr, wlr_pointer_button_event* e) {
    // HyprlandAPI::addNotification(PHANDLE, "Mouse down normal!", CColor{0.8f, 0.2f, 0.5f, 1.0f}, 5000);
    (*(origMouseDownNormal)g_pMouseDownHook->m_pOriginal)(thisptr, e);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addNotification(PHANDLE, "Hello World from an example plugin!", CColor{0.f, 1.f, 1.f, 1.f}, 5000);

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "activeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onActiveWindowChange(self, data); });
    HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); });

    g_pCustomLayout = std::make_unique<CHyprCustomLayout>();

    HyprlandAPI::addLayout(PHANDLE, "custom", g_pCustomLayout.get());

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:example:border_color", SConfigValue{.intValue = configStringToInt("rgb(44ee44)")});

    HyprlandAPI::addDispatcher(PHANDLE, "example", [](std::string arg) { HyprlandAPI::addNotification(PHANDLE, "Arg passed: " + arg, CColor{0.5f, 0.5f, 0.7f, 1.0f}, 5000); });

    // Hook a public member
    g_pFocusHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CCompositor::focusWindow, (void*)&hkFocusWindow);
    // Hook a public non-member
    g_pMotionHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&wlr_seat_pointer_notify_motion, (void*)&hkNotifyMotion);
    // Hook a private member
    static const auto METHODS = HyprlandAPI::findFunctionsByName(PHANDLE, "processMouseDownNormal");
    g_pMouseDownHook          = HyprlandAPI::createFunctionHook(PHANDLE, METHODS[0].address, (void*)&hkProcessMouseDownNormal);

    static auto* const PBORDERCOLOR = HyprlandAPI::getConfigValue(PHANDLE, "plugin:example:border_color");

    // fancy notifications
    HyprlandAPI::addNotificationV2(
        PHANDLE,
        {{"text", "Example hint, color " + std::to_string(PBORDERCOLOR->intValue)}, {"time", (uint64_t)10000}, {"color", CColor{PBORDERCOLOR->intValue}}, {"icon", ICON_HINT}});

    // Enable our hooks
    g_pFocusHook->hook();
    g_pMotionHook->hook();
    g_pMouseDownHook->hook();

    HyprlandAPI::reloadConfig();

    return {"ExamplePlugin", "An example plugin", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::invokeHyprctlCommand("seterror", "disable");
}