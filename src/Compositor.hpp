#pragma once

#include <memory>

#include "defines.hpp"
#include "debug/Log.hpp"
#include "events/Events.hpp"
#include "config/ConfigManager.hpp"
#include "ManagerThread.hpp"
#include "input/InputManager.hpp"

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
    // ------------------------------------------------- //


    const char*             m_szWLDisplaySocket;

    void            startCompositor();    
};


inline std::unique_ptr<CCompositor> g_pCompositor;