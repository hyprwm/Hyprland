#pragma once

#include "../helpers/AnimatedVariable.hpp"
#include <string>
#include <unordered_set>
#include "DesktopTypes.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "../helpers/signal/Signal.hpp"

namespace Layout {
    class CSpace;
};

class CWorkspace {
  public:
    static PHLWORKSPACE create(WORKSPACEID id, PHLMONITOR monitor, std::string name, bool special = false, bool isEmpty = true);
    // use create() don't use this
    CWorkspace(WORKSPACEID id, PHLMONITOR monitor, std::string name, bool special = false, bool isEmpty = true);
    ~CWorkspace();

    WP<CWorkspace>     m_self;

    SP<Layout::CSpace> m_space;

    // Workspaces ID-based have IDs > 0
    // and workspaces name-based have IDs starting with -1337
    WORKSPACEID   m_id   = WORKSPACE_INVALID;
    std::string   m_name = "";
    PHLMONITORREF m_monitor;

    // for animations
    PHLANIMVAR<Vector2D>       m_renderOffset;
    PHLANIMVAR<float>          m_alpha;
    bool                       m_forceRendering = false;
    std::optional<std::string> m_animationStyle;

    // allows damage to propagate.
    bool m_visible = false;

    // "scratchpad"
    bool m_isSpecialWorkspace = false;

    // last window
    PHLWINDOWREF m_lastFocusedWindow;

    // last monitor (used on reconnect)
    std::string m_lastMonitor = "";

    bool        m_wasCreatedEmpty = true;

    // Inert: destroyed and invalid. If this is true, release the ptr you have.
    bool                          inert();
    MONITORID                     monitorID();
    PHLWINDOW                     getLastFocusedWindow();
    PHLWINDOW                     getFocusCandidate();
    std::string                   getConfigName();
    bool                          matchesStaticSelector(const std::string& selector);
    void                          markInert();
    void                          updateWindowDecos();
    void                          updateWindowData();
    std::unordered_set<PHLWINDOW> getWindows(std::optional<bool> onlyTiled = {}, std::optional<bool> onlyPinned = {}, std::optional<bool> onlyVisible = {});
    int         getWindowCount(std::optional<bool> onlyTiled = {}, std::optional<bool> onlyPinned = {}, std::optional<bool> onlyVisible = {});
    int         getGroups(std::optional<bool> onlyTiled = {}, std::optional<bool> onlyPinned = {}, std::optional<bool> onlyVisible = {});
    bool        hasUrgentWindow();
    PHLWINDOW   getFirstWindow();
    PHLWINDOW   getTopLeftWindow();
    // PHLWINDOW   getFullscreenWindow(); // ERSTARR TODO - remove this
    bool        isVisible();
    bool        isVisibleNotCovered();
    void        rename(const std::string& name = "");
    void        changeID(int64_t id);
    void        forceReportSizesToWindows();
    void        updateWindows();
    void        setPersistent(bool persistent);
    bool        isPersistent();

    struct {
        CSignalT<> destroy;
        CSignalT<> renamed;
        CSignalT<> idChanged;
        CSignalT<> monitorChanged;
        CSignalT<> activeChanged;
    } m_events;

  private:
    void                init(PHLWORKSPACE self);

    CHyprSignalListener m_focusedWindowHook;
    bool                m_inert = true;

    SP<CWorkspace>      m_selfPersistent; // for persistent workspaces.
    bool                m_persistent = false;
    bool                m_wasRenamed = false;
};

inline bool valid(const PHLWORKSPACE& ref) {
    if (!ref)
        return false;

    return !ref->inert();
}
