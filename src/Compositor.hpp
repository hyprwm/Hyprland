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
#include "helpers/Monitor.hpp"
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
    wlr_seat*                        m_sWLRSeat;
    wlr_output_manager_v1*           m_sWLROutputMgr;
    wlr_presentation*                m_sWLRPresentation;
    wlr_scene*                       m_sWLRScene;
    // ------------------------------------------------- //


    const char*             m_szWLDisplaySocket;

    std::list<SMonitor>     m_lMonitors;
    std::list<CWindow>      m_lWindows;

    void                    startCompositor(); 


    // ------------------------------------------------- //

    SMonitor*               getMonitorFromID(const int&);
    SMonitor*               getMonitorFromCursor();
    void                    removeWindowFromVectorSafe(CWindow*);
};


inline std::unique_ptr<CCompositor> g_pCompositor;