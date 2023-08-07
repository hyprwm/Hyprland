#pragma once

#include "AnimatedVariable.hpp"
#include <string>
#include "../defines.hpp"
#include "../wlrunstable/wlr_ext_workspace_v1.hpp"

enum eFullscreenMode : uint8_t {
    FULLSCREEN_FULL = 0,
    FULLSCREEN_MAXIMIZED
};

class CWindow;

class CWorkspace {
  public:
    CWorkspace(int monitorID, std::string name, bool special = false);
    ~CWorkspace();

    // Workspaces ID-based have IDs > 0
    // and workspaces name-based have IDs starting with -1337
    int         m_iID        = -1;
    std::string m_szName     = "";
    uint64_t    m_iMonitorID = -1;
    // Previous workspace ID is stored during a workspace change, allowing travel
    // to the previous workspace.
    struct SPrevWorkspaceData {
        int         iID  = -1;
        std::string name = "";
    } m_sPrevWorkspace;

    bool                         m_bHasFullscreenWindow = false;
    eFullscreenMode              m_efFullscreenMode     = FULLSCREEN_FULL;

    wlr_ext_workspace_handle_v1* m_pWlrHandle = nullptr;

    wl_array                     m_wlrCoordinateArr;

    // for animations
    CAnimatedVariable m_vRenderOffset;
    CAnimatedVariable m_fAlpha;
    bool              m_bForceRendering = false;

    // "scratchpad"
    bool m_bIsSpecialWorkspace = false;

    // last window
    CWindow* m_pLastFocusedWindow = nullptr;

    // user-set
    bool m_bDefaultFloating = false;
    bool m_bDefaultPseudo   = false;

    // don't destroy in sanity check
    bool m_bIndestructible = false;

    // last monitor (used on reconnect)
    std::string m_szLastMonitor = "";

    void        startAnim(bool in, bool left, bool instant = false);
    void        setActive(bool on);

    void        moveToMonitor(const int&);

    CWindow*    getLastFocusedWindow();
    void        rememberPrevWorkspace(const CWorkspace* prevWorkspace);

    std::string getConfigName();
};
