#include "WorkspaceWindowFocusTracker.hpp"
#include "HLWorkspace.hpp"

#include "../event/EventBus.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/state/FocusState.hpp"

using namespace Workspace;

CWorkspaceWindowFocusTracker::CWorkspaceWindowFocusTracker(CHLWorkspace* ws) : m_parent(ws) {
    m_listener = Event::bus()->m_events.window.active.listen([this](PHLWINDOW w, Desktop::eFocusReason) {
        if (w && w->m_workspace.get() == m_parent)
            remember(w);
    });
}

PHLWINDOW CWorkspaceWindowFocusTracker::last() const {
    return m_last.lock();
}

void CWorkspaceWindowFocusTracker::remember(PHLWINDOW window) {
    m_last = window;
}
