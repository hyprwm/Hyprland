#pragma once

#include "../helpers/AnimatedVariable.hpp"
#include <string>
#include "DesktopTypes.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "../helpers/signal/Signal.hpp"

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

    WP<CWorkspace> m_self;

    // Workspaces ID-based have IDs > 0
    // and workspaces name-based have IDs starting with -1337
    WORKSPACEID     m_id   = WORKSPACE_INVALID;
    std::string     m_name = "";
    PHLMONITORREF   m_monitor;

    bool            m_hasFullscreenWindow = false;
    eFullscreenMode m_fullscreenMode      = FSMODE_NONE;

    wl_array        m_wlrCoordinateArr;

    // for animations
    PHLANIMVAR<Vector2D> m_renderOffset;
    PHLANIMVAR<float>    m_alpha;
    bool                 m_forceRendering = false;

    // allows damage to propagate.
    bool m_visible = false;

    // "scratchpad"
    bool m_isSpecialWorkspace = false;

    // last window
    PHLWINDOWREF m_lastFocusedWindow;

    // user-set
    bool m_defaultFloating = false;
    bool m_defaultPseudo   = false;

    // last monitor (used on reconnect)
    std::string m_lastMonitor = "";

    bool        m_wasCreatedEmpty = true;

    bool        m_persistent = false;

    // Inert: destroyed and invalid. If this is true, release the ptr you have.
    bool             inert();
    void             startAnim(bool in, bool left, bool instant = false);
    MONITORID        monitorID();
    PHLWINDOW        getLastFocusedWindow();
    void             rememberPrevWorkspace(const PHLWORKSPACE& prevWorkspace);
    std::string      getConfigName();
    bool             matchesStaticSelector(const std::string& selector);
    void             markInert();
    SWorkspaceIDName getPrevWorkspaceIDName() const;
    void             updateWindowDecos();
    void             updateWindowData();
    int              getWindows(std::optional<bool> onlyTiled = {}, std::optional<bool> onlyPinned = {}, std::optional<bool> onlyVisible = {});
    int              getGroups(std::optional<bool> onlyTiled = {}, std::optional<bool> onlyPinned = {}, std::optional<bool> onlyVisible = {});
    bool             hasUrgentWindow();
    PHLWINDOW        getFirstWindow();
    PHLWINDOW        getTopLeftWindow();
    PHLWINDOW        getFullscreenWindow();
    bool             isVisible();
    bool             isVisibleNotCovered();
    void             rename(const std::string& name = "");
    void             forceReportSizesToWindows();
    void             updateWindows();

    struct {
        CSignalT<> destroy;
        CSignalT<> renamed;
        CSignalT<> monitorChanged;
        CSignalT<> activeChanged;
    } m_events;

  private:
    void init(PHLWORKSPACE self);
    // Previous workspace ID and name is stored during a workspace change, allowing travel
    // to the previous workspace.
    SWorkspaceIDName     m_prevWorkspace;

    SP<HOOK_CALLBACK_FN> m_focusedWindowHook;
    bool                 m_inert = true;
};

inline bool valid(const PHLWORKSPACE& ref) {
    if (!ref)
        return false;

    return !ref->inert();
}
