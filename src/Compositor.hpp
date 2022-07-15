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
    wl_event_loop*                   m_sWLEventLoop;
    wlr_backend*                     m_sWLRBackend;
    wlr_session*                     m_sWLRSession;
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
    wlr_xdg_foreign_registry*        m_sWLRForeignRegistry;
    wlr_idle_inhibit_manager_v1*     m_sWLRIdleInhibitMgr;
    wlr_pointer_gestures_v1*         m_sWLRPointerGestures;
    // ------------------------------------------------- //


    const char*             m_szWLDisplaySocket;
    std::string             m_szInstanceSignature = "";
    std::string             m_szCurrentSplash = "error";

    std::vector<std::unique_ptr<SMonitor>>      m_vMonitors;
    std::vector<std::unique_ptr<CWindow>>       m_vWindows;
    std::deque<std::unique_ptr<CWindow>>        m_dUnmanagedX11Windows;
    std::vector<std::unique_ptr<SXDGPopup>>     m_vXDGPopups;
    std::vector<std::unique_ptr<CWorkspace>>    m_vWorkspaces;
    std::vector<std::unique_ptr<SSubsurface>>   m_vSubsurfaces;
    std::vector<CWindow*>                       m_vWindowsFadingOut;
    std::vector<SLayerSurface*>                 m_vSurfacesFadingOut;

    void                    startCompositor(); 
    void                    cleanup();

    wlr_surface*            m_pLastFocus = nullptr;
    CWindow*                m_pLastWindow = nullptr;
    SMonitor*               m_pLastMonitor = nullptr;
    
    SSeat                   m_sSeat;

    bool                    m_bReadyToProcess = false;
    bool                    m_bSessionActive = true;

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
    CWindow*                vectorToWindowIdeal(const Vector2D&); // used only for finding a window to focus on, basically a "findFocusableWindow"
    CWindow*                vectorToWindowTiled(const Vector2D&);
    wlr_surface*            vectorToLayerSurface(const Vector2D&, std::list<SLayerSurface*>*, Vector2D*, SLayerSurface**);
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
    void                    updateWorkspaceWindowDecos(const int&);
    int                     getWindowsOnWorkspace(const int&);
    CWindow*                getFirstWindowOnWorkspace(const int&);
    void                    fixXWaylandWindowsOnWorkspace(const int&);
    CWindow*                getFullscreenWindowOnWorkspace(const int&);
    bool                    doesSeatAcceptInput(wlr_surface*);
    bool                    isWindowActive(CWindow*);
    void                    moveWindowToTop(CWindow*);
    void                    cleanupFadingOut(const int& monid);
    CWindow*                getWindowInDirection(CWindow*, char);
    void                    deactivateAllWLRWorkspaces(wlr_ext_workspace_handle_v1* exclude = nullptr);
    CWindow*                getNextWindowOnWorkspace(CWindow*);
    CWindow*                getPrevWindowOnWorkspace(CWindow*);
    int                     getNextAvailableNamedWorkspace();
    bool                    isPointOnAnyMonitor(const Vector2D&);
    CWindow*                getConstraintWindow(SMouse*);
    SMonitor*               getMonitorInDirection(const char&);
    void                    updateAllWindowsAnimatedDecorationValues();
    void                    updateWindowAnimatedDecorationValues(CWindow*);
    void                    moveWindowToWorkspace(CWindow*, const std::string&);
    int                     getNextAvailableMonitorID();
    void                    moveWorkspaceToMonitor(CWorkspace*, SMonitor*);
    bool                    workspaceIDOutOfBounds(const int&);
    void                    setWindowFullscreen(CWindow*, bool, eFullscreenMode);
    void                    moveUnmanagedX11ToWindows(CWindow*);
    CWindow*                getX11Parent(CWindow*);
    void                    scheduleFrameForMonitor(SMonitor*);

    std::string             explicitConfigPath;

private:
    void                    initAllSignals();
    void                    setRandomSplash();
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