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

        void                   fullWindowFocus(PHLWINDOW w, SP<CWLSurfaceResource> surface = nullptr, bool forceFSCycle = false);
        void                   rawWindowFocus(PHLWINDOW w, SP<CWLSurfaceResource> surface = nullptr);
        void                   rawSurfaceFocus(SP<CWLSurfaceResource> s, PHLWINDOW pWindowOwner = nullptr);
        void                   rawMonitorFocus(PHLMONITOR m);

        void                   resetWindowFocus();

        SP<CWLSurfaceResource> surface();
        PHLWINDOW              window();
        PHLMONITOR             monitor();

      private:
        WP<CWLSurfaceResource> m_focusSurface;
        PHLWINDOWREF           m_focusWindow;
        PHLMONITORREF          m_focusMonitor;

        SP<HOOK_CALLBACK_FN>   m_windowOpen, m_windowClose;
    };

    SP<CFocusState> focusState();
};
