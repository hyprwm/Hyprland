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
#include "managers/input/InputManager.hpp"
#include "managers/LayoutManager.hpp"
#include "managers/KeybindManager.hpp"
#include "managers/AnimationManager.hpp"
#include "managers/EventManager.hpp"
#include "debug/HyprDebugOverlay.hpp"
#include "helpers/Monitor.hpp"
#include "helpers/Workspace.hpp"
#include "Window.hpp"
#include "render/Renderer.hpp"
#include "render/OpenGL.hpp"
#include "hyprerror/HyprError.hpp"

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
    int                              m_iDRMFD;
    wlr_ext_workspace_manager_v1*    m_sWLREXTWorkspaceMgr;
    wlr_linux_dmabuf_v1*             m_sWLRDmabuf;
    wlr_pointer_constraints_v1*      m_sWLRPointerConstraints;
    wlr_relative_pointer_manager_v1* m_sWLRRelPointerMgr;
    wlr_server_decoration_manager*   m_sWLRServerDecoMgr;
    wlr_xdg_decoration_manager_v1*   m_sWLRXDGDecoMgr;
    wlr_virtual_pointer_manager_v1*  m_sWLRVirtPtrMgr;
    wlr_foreign_toplevel_manager_v1* m_sWLRToplevelMgr;
    wlr_tablet_manager_v2*           m_sWLRTabletManager;
    // ------------------------------------------------- //


    const char*             m_szWLDisplaySocket;
    std::string             m_szInstanceSignature = "";

    std::list<SMonitor>     m_lMonitors;
    std::list<CWindow>      m_lWindows;
    std::list<SXDGPopup>    m_lXDGPopups;
    std::list<CWorkspace>   m_lWorkspaces;
    std::list<SSubsurface>  m_lSubsurfaces;
    std::list<CWindow*>     m_lWindowsFadingOut;
    std::list<SLayerSurface*> m_lSurfacesFadingOut;

    void                    startCompositor(); 
    void                    cleanupExit();

    wlr_surface*            m_pLastFocus = nullptr;
    CWindow*                m_pLastWindow = nullptr;
    SMonitor*               m_pLastMonitor = nullptr;
    
    SSeat                   m_sSeat;

    bool                    m_bReadyToProcess = false;

    // ------------------------------------------------- //

    SMonitor*               getMonitorFromID(const int&);
    SMonitor*               getMonitorFromName(const std::string&);
    SMonitor*               getMonitorFromCursor();
    SMonitor*               getMonitorFromVector(const Vector2D&);
    void                    removeWindowFromVectorSafe(CWindow*);
    void                    focusWindow(CWindow*, wlr_surface* pSurface = nullptr);
    void                    focusSurface(wlr_surface*, CWindow* pWindowOwner = nullptr);
    bool                    windowExists(CWindow*);
    bool                    windowValidMapped(CWindow*);
    CWindow*                vectorToWindow(const Vector2D&);
    CWindow*                vectorToWindowIdeal(const Vector2D&);
    CWindow*                vectorToWindowTiled(const Vector2D&);
    wlr_surface*            vectorToLayerSurface(const Vector2D&, std::list<SLayerSurface*>*, Vector2D*);
    wlr_surface*            vectorWindowToSurface(const Vector2D&, CWindow*, Vector2D& sl);
    CWindow*                windowFromCursor();
    CWindow*                windowFloatingFromCursor();
    SMonitor*               getMonitorFromOutput(wlr_output*);
    CWindow*                getWindowForPopup(wlr_xdg_popup*);
    CWindow*                getWindowFromSurface(wlr_surface*);
    bool                    isWorkspaceVisible(const int&);
    CWorkspace*             getWorkspaceByID(const int&);
    CWorkspace*             getWorkspaceByName(const std::string&);
    CWorkspace*             getWorkspaceByString(const std::string&);
    void                    sanityCheckWorkspaces();
    int                     getWindowsOnWorkspace(const int&);
    CWindow*                getFirstWindowOnWorkspace(const int&);
    void                    fixXWaylandWindowsOnWorkspace(const int&);
    CWindow*                getFullscreenWindowOnWorkspace(const int&);
    bool                    doesSeatAcceptInput(wlr_surface*);
    bool                    isWindowActive(CWindow*);
    void                    moveWindowToTop(CWindow*);
    void                    cleanupFadingOut();
    CWindow*                getWindowInDirection(CWindow*, char);
    void                    deactivateAllWLRWorkspaces(wlr_ext_workspace_handle_v1* exclude = nullptr);
    CWindow*                getNextWindowOnWorkspace(CWindow*);
    int                     getNextAvailableNamedWorkspace();
    bool                    isPointOnAnyMonitor(const Vector2D&);
    CWindow*                getConstraintWindow(SMouse*);
    SMonitor*               getMonitorInDirection(const char&);
    void                    updateAllWindowsBorders();
    void                    updateWindowBorderColor(CWindow*);
    void                    moveWindowToWorkspace(CWindow*, const int);
    int                     getNextAvailableMonitorID();
    void                    moveWorkspaceToMonitor(CWorkspace*, SMonitor*);
    bool                    workspaceIDOutOfBounds(const int&);

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