#pragma once

#include "../helpers/AnimatedVariable.hpp"
#include <string>
#include "../defines.hpp"
#include "DesktopTypes.hpp"
#include "../layout/IHyprLayout.hpp"

enum eFullscreenMode : int8_t {
    FULLSCREEN_INVALID = -1,
    FULLSCREEN_FULL    = 0,
    FULLSCREEN_MAXIMIZED
};

class CWindow;

class CWorkspace {
  public:
    static PHLWORKSPACE create(int id, int monitorID, std::string name, bool special = false, bool isEmtpy = true);
    // use create() don't use this
    CWorkspace(int id, int monitorID, std::string name, bool special = false, bool isEmpty = true);
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

    bool            m_bHasFullscreenWindow = false;
    eFullscreenMode m_efFullscreenMode     = FULLSCREEN_FULL;

    wl_array        m_wlrCoordinateArr;

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

    bool        m_bWasCreatedEmtpy = true;

    bool        m_bPersistent = false;

    // Inert: destroyed and invalid. If this is true, release the ptr you have.
    bool         inert();

    void         startAnim(bool in, bool left, bool instant = false);
    void         setActive(bool on);

    void         moveToMonitor(const int&);

    PHLWINDOW    getLastFocusedWindow();
    void         rememberPrevWorkspace(const PHLWORKSPACE& prevWorkspace);

    std::string  getConfigName();

    bool         matchesStaticSelector(const std::string& selector);

    void         markInert();

    IHyprLayout* getCurrentLayout();

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
