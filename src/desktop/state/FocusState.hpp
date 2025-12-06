#pragma once

#include "../DesktopTypes.hpp"
#include "../../SharedDefs.hpp"

class CWLSurfaceResource;

namespace Desktop {
    class CFocusState {
      public:
        CFocusState();
        ~CFocusState() = default;

        CFocusState(CFocusState&&)      = delete;
        CFocusState(CFocusState&)       = delete;
        CFocusState(const CFocusState&) = delete;

        void                             fullWindowFocus(PHLWINDOW w, SP<CWLSurfaceResource> surface = nullptr, bool preserveFocusHistory = false, bool forceFSCycle = false);
        void                             rawWindowFocus(PHLWINDOW w, SP<CWLSurfaceResource> surface = nullptr, bool preserveFocusHistory = false);
        void                             rawSurfaceFocus(SP<CWLSurfaceResource> s, PHLWINDOW pWindowOwner = nullptr);
        void                             rawMonitorFocus(PHLMONITOR m);

        SP<CWLSurfaceResource>           surface();
        PHLWINDOW                        window();
        PHLMONITOR                       monitor();
        const std::vector<PHLWINDOWREF>& windowHistory();

        void                             addWindowToHistory(PHLWINDOW w);

      private:
        void                      removeWindowFromHistory(PHLWINDOW w);
        void                      moveWindowToLatestInHistory(PHLWINDOW w);

        WP<CWLSurfaceResource>    m_focusSurface;
        PHLWINDOWREF              m_focusWindow;
        PHLMONITORREF             m_focusMonitor;
        std::vector<PHLWINDOWREF> m_windowFocusHistory; // first element is the most recently focused

        SP<HOOK_CALLBACK_FN>      m_windowOpen, m_windowClose;
    };

    SP<CFocusState> focusState();
};
