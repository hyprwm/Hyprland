#pragma once

#include "../defines.hpp"
#include "AnimatedVariable.hpp"

enum eFullscreenMode : uint8_t {
    FULLSCREEN_FULL = 0,
    FULLSCREEN_MAXIMIZED
};

class CWorkspace {
public:
    CWorkspace(int monitorID, std::string name, bool special = false);
    ~CWorkspace();

    // Workspaces ID-based have IDs > 0
    // and workspaces name-based have IDs starting with -1337
    int             m_iID = -1;
    std::string     m_szName = "";
    uint64_t        m_iMonitorID = -1;
    bool            m_bHasFullscreenWindow = false;
    eFullscreenMode m_efFullscreenMode = FULLSCREEN_FULL;

    wlr_ext_workspace_handle_v1* m_pWlrHandle = nullptr;

    wl_array        m_wlrCoordinateArr;

    // for animations
    CAnimatedVariable m_vRenderOffset;
    CAnimatedVariable m_fAlpha;
    bool              m_bForceRendering = false;

    // "scratchpad"
    bool            m_bIsSpecialWorkspace = false;

    // user-set
    bool            m_bDefaultFloating = false;
    bool            m_bDefaultPseudo = false;

    void            startAnim(bool in, bool left, bool instant = false);
    void            setActive(bool on);

    void            moveToMonitor(const int&);
};