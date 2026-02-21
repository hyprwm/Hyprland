#pragma once

#include "../DesktopTypes.hpp"
#include "../../SharedDefs.hpp"

class CWLSurfaceResource;

namespace Desktop {
    enum eFocusReason : uint8_t {
        FOCUS_REASON_UNKNOWN = 0,
        FOCUS_REASON_FFM,
        FOCUS_REASON_KEYBIND,
        FOCUS_REASON_CLICK,
        FOCUS_REASON_OTHER,
        FOCUS_REASON_DESKTOP_STATE_CHANGE,
        FOCUS_REASON_NEW_WINDOW,
        FOCUS_REASON_GHOSTS,
    };

    bool isHardInputFocusReason(eFocusReason r);

    class CFocusState {
      public:
        CFocusState();
        ~CFocusState() = default;

        CFocusState(CFocusState&&)      = delete;
        CFocusState(CFocusState&)       = delete;
        CFocusState(const CFocusState&) = delete;

        void                   fullWindowFocus(PHLWINDOW w, eFocusReason reason, SP<CWLSurfaceResource> surface = nullptr, bool forceFSCycle = false);
        void                   rawWindowFocus(PHLWINDOW w, eFocusReason reason, SP<CWLSurfaceResource> surface = nullptr);
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
