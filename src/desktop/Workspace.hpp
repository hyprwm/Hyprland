#pragma once

#include "../helpers/AnimatedVariable.hpp"
#include <string>
#include "../defines.hpp"
#include "DesktopTypes.hpp"
#include "../helpers/MiscFunctions.hpp"

enum eFullscreenMode : int8_t {
    FSMODE_NONE       = 0,
    FSMODE_MAXIMIZED  = 1 << 0,
    FSMODE_FULLSCREEN = 1 << 1,
    FSMODE_MAX        = (1 << 2) - 1
};

class CWindow;

class CWorkspace {
  public:
    static PHLWORKSPACE create(WORKSPACEID id, PHLMONITOR monitor, std::string name, bool special = false, bool isEmpty = true);
    // use create() don't use this
    CWorkspace(WORKSPACEID id, PHLMONITOR monitor, std::string name, bool special = false, bool isEmpty = true);
    ~CWorkspace();

    // Workspaces ID-based have IDs > 0
    // and workspaces name-based have IDs starting with -1337
    WORKSPACEID   m_iID    = WORKSPACE_INVALID;
    std::string   m_szName = "";
    PHLMONITORREF m_pMonitor;
    // Previous workspace ID and name is stored during a workspace change, allowing travel
    // to the previous workspace.
    SWorkspaceIDName m_sPrevWorkspace, m_sPrevWorkspacePerMonitor;

    bool             m_bHasFullscreenWindow = false;
    eFullscreenMode  m_efFullscreenMode     = FSMODE_NONE;

    wl_array         m_wlrCoordinateArr;

    // for animations
    CAnimatedVariable<Vector2D> m_vRenderOffset;
    CAnimatedVariable<float>    m_fAlpha;
    bool                        m_bForceRendering = false;

    // allows damage to propagate.
    bool m_bVisible = false;

    // "scratchpad"
    bool m_bIsSpecialWorkspace = false;

    // last window
    PHLWINDOWREF m_pLastFocusedWindow;

    // user-set
    bool m_bDefaultFloating = false;
    bool m_bDefaultPseudo   = false;

    // last monitor (used on reconnect)
    std::string m_szLastMonitor = "";

    bool        m_bWasCreatedEmpty = true;

    bool        m_bPersistent = false;

    // Inert: destroyed and invalid. If this is true, release the ptr you have.
    bool             inert();
    void             startAnim(bool in, bool left, bool instant = false);
    void             setActive(bool on);
    void             moveToMonitor(const MONITORID&);
    MONITORID        monitorID();
    PHLWINDOW        getLastFocusedWindow();
    void             rememberPrevWorkspace(const PHLWORKSPACE& prevWorkspace);
    std::string      getConfigName();
    bool             matchesStaticSelector(const std::string& selector);
    void             markInert();
    SWorkspaceIDName getPrevWorkspaceIDName(bool perMonitor) const;
    void             updateWindowDecos();
    void             updateWindowData();
    int              getWindows(std::optional<bool> onlyTiled = {}, std::optional<bool> onlyVisible = {});
    int              getGroups(std::optional<bool> onlyTiled = {}, std::optional<bool> onlyVisible = {});
    bool             hasUrgentWindow();
    PHLWINDOW        getFirstWindow();
    PHLWINDOW        getTopLeftWindow();
    PHLWINDOW        getFullscreenWindow();
    bool             isVisible();
    bool             isVisibleNotCovered();
    void             rename(const std::string& name = "");
    void             forceReportSizesToWindows();
    void             updateWindows();

  private:
    void                 init(PHLWORKSPACE self);

    SP<HOOK_CALLBACK_FN> m_pFocusedWindowHook;
    bool                 m_bInert = true;
    WP<CWorkspace>       m_pSelf;
};

inline bool valid(const PHLWORKSPACE& ref) {
    if (!ref)
        return false;

    return !ref->inert();
}
