#pragma once

#include <memory>
#include <deque>
#include <list>

#include "defines.hpp"
#include "debug/Log.hpp"
#include "events/Events.hpp"
#include "config/ConfigManager.hpp"
#include "managers/ThreadManager.hpp"
#include "managers/XWaylandManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/LayoutManager.hpp"
#include "managers/KeybindManager.hpp"
#include "managers/AnimationManager.hpp"
#include "helpers/Monitor.hpp"
#include "helpers/Workspace.hpp"
#include "Window.hpp"
#include "render/Renderer.hpp"

class CCompositor {
public:
    CCompositor();
    ~CCompositor();

    // ------------------ WLR BASICS ------------------ //
    wl_display*                      m_sWLDisplay;
    wlr_backend*                     m_sWLRBackend;
    wlr_renderer*                    m_sWLRRenderer;
    wlr_allocator*                   m_sWLRAllocator;
    wlr_compositor*                  m_sWLRCompositor;
    wlr_subcompositor*               m_sWLRSubCompositor;
    wlr_data_device_manager*         m_sWLRDataDevMgr;
    wlr_xdg_activation_v1*           m_sWLRXDGActivation;
    wlr_output_layout*               m_sWLROutputLayout;
    wlr_idle*                        m_sWLRIdle;
    wlr_layer_shell_v1*              m_sWLRLayerShell;
    wlr_xdg_shell*                   m_sWLRXDGShell;
    wlr_cursor*                      m_sWLRCursor;
    wlr_xcursor_manager*             m_sWLRXCursorMgr;
    wlr_virtual_keyboard_manager_v1* m_sWLRVKeyboardMgr;
    wlr_output_manager_v1*           m_sWLROutputMgr;
    wlr_presentation*                m_sWLRPresentation;
    wlr_scene*                       m_sWLRScene;
    wlr_input_inhibit_manager*       m_sWLRInhibitMgr;
    wlr_keyboard_shortcuts_inhibit_manager_v1* m_sWLRKbShInhibitMgr;
    wlr_egl*                         m_sWLREGL;
    // ------------------------------------------------- //


    const char*             m_szWLDisplaySocket;

    std::list<SMonitor>     m_lMonitors;
    std::list<CWindow>      m_lWindows;
    std::list<SLayerPopup>  m_lLayerPopups;
    std::list<SXDGPopup>    m_lXDGPopups;
    std::list<SWorkspace>   m_lWorkspaces;
    std::list<SSubsurface>  m_lSubsurfaces;

    void                    startCompositor(); 

    wlr_surface*            m_pLastFocus = nullptr;
    SMonitor*               m_pLastMonitor = nullptr;
    
    SSeat                   m_sSeat;

    // ------------------------------------------------- //

    SMonitor*               getMonitorFromID(const int&);
    SMonitor*               getMonitorFromCursor();
    SMonitor*               getMonitorFromVector(const Vector2D&);
    void                    removeWindowFromVectorSafe(CWindow*);
    void                    focusWindow(CWindow*);
    void                    focusSurface(wlr_surface*);
    bool                    windowExists(CWindow*);
    bool                    windowValidMapped(CWindow*);
    CWindow*                vectorToWindow(const Vector2D&);
    CWindow*                vectorToWindowIdeal(const Vector2D&);
    CWindow*                vectorToWindowTiled(const Vector2D&);
    wlr_surface*            vectorToLayerSurface(const Vector2D&, std::list<SLayerSurface*>*, Vector2D*);
    CWindow*                windowFromCursor();
    CWindow*                windowFloatingFromCursor();
    SMonitor*               getMonitorFromOutput(wlr_output*);
    SLayerSurface*          getLayerForPopup(SLayerPopup*);
    CWindow*                getWindowForPopup(wlr_xdg_popup*);
    CWindow*                getWindowFromSurface(wlr_surface*);
    bool                    isWorkspaceVisible(const int&);
    SWorkspace*             getWorkspaceByID(const int&);
    void                    sanityCheckWorkspaces();
    int                     getWindowsOnWorkspace(const int&);
    CWindow*                getFirstWindowOnWorkspace(const int&);
    void                    fixXWaylandWindowsOnWorkspace(const int&);
    CWindow*                getFullscreenWindowOnWorkspace(const int&);
    bool                    doesSeatAcceptInput(wlr_surface*);

private:
    void                    initAllSignals();
};


inline std::unique_ptr<CCompositor> g_pCompositor;

// For XWayland
inline std::map<std::string, xcb_atom_t> HYPRATOMS = {
    HYPRATOM("_NET_WM_WINDOW_TYPE"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_NORMAL"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DOCK"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DIALOG"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_UTILITY"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_TOOLBAR"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_SPLASH"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_MENU"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_POPUP_MENU"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_TOOLTIP"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_NOTIFICATION")};