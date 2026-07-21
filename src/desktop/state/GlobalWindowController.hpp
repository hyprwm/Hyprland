#pragma once

#include "../DesktopTypes.hpp"

namespace Desktop {
    class CGlobalWindowController {
      public:
        void updateAllWindowsDecorations() const;
        void updateSuspendedStates() const;
        void moveWindowToWorkspace(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace) const;
    };

    UP<CGlobalWindowController>& globalWindowController();
};