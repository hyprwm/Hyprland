#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/signal/Signal.hpp"

namespace Workspace {
    class CHLWorkspace;

    class CWorkspaceWindowFocusTracker {
      public:
        explicit CWorkspaceWindowFocusTracker(CHLWorkspace*);
        ~CWorkspaceWindowFocusTracker() = default;

        CWorkspaceWindowFocusTracker(const CWorkspaceWindowFocusTracker&) = delete;
        CWorkspaceWindowFocusTracker(CWorkspaceWindowFocusTracker&&)      = delete;

        CWorkspaceWindowFocusTracker& operator=(const CWorkspaceWindowFocusTracker&) = delete;
        CWorkspaceWindowFocusTracker& operator=(CWorkspaceWindowFocusTracker&&)      = delete;

        PHLWINDOW                     last() const;
        void                          remember(PHLWINDOW window);

      private:
        CHLWorkspace*       m_parent;

        PHLWINDOWREF        m_last;
        CHyprSignalListener m_listener;
    };
}
