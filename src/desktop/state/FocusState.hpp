#pragma once

#include "../DesktopTypes.hpp"
#include "../../helpers/signal/Signal.hpp"

class CWLSurfaceResource;

namespace Desktop {
    enum eFocusReason : uint32_t {
        FOCUS_REASON_UNKNOWN                      = 0,
        FOCUS_REASON_FFM                          = 1 << 0,
        FOCUS_REASON_KEYBIND                      = 1 << 1,
        FOCUS_REASON_DISPATCH_FOCUSWINDOW         = 1 << 2,
        FOCUS_REASON_DISPATCH_MOVEWINDOWINTOGROUP = 1 << 3,
        FOCUS_REASON_CLICK                        = 1 << 4,
        FOCUS_REASON_CLICK_DOWN                   = (1 << 5) | FOCUS_REASON_CLICK,
        FOCUS_REASON_CLICK_UP                     = (1 << 6) | FOCUS_REASON_CLICK,
        FOCUS_REASON_DESKTOP_STATE_CHANGE         = 1 << 7,
        FOCUS_REASON_SWITCH_TO_WINDOW_SOFT        = 1 << 8,
        FOCUS_REASON_SWITCH_TO_WINDOW_HARD        = 1 << 9,
        FOCUS_REASON_GROUP_CURRENT_WINDOW_CHANGE  = 1 << 10,
        FOCUS_REASON_WORKSPACE_CHANGE             = 1 << 11,
        FOCUS_REASON_TOGGLE_SPECIAL_WORKSPACE     = 1 << 12,
        FOCUS_REASON_UNMAP_WINDOW_TILING          = 1 << 13,
        FOCUS_REASON_UNMAP_WINDOW_FLOATING        = 1 << 14,
        FOCUS_REASON_UNMAP_GROUPED_WINDOW         = 1 << 15,
        FOCUS_REASON_NEW_WINDOW                   = 1 << 16,
        FOCUS_REASON_GHOSTS                       = 1 << 17,
        FOCUS_REASON_OTHER                        = 1 << 18,
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

        bool                   isWindowActive(PHLWINDOW w) const;

        SP<CWLSurfaceResource> surface();
        PHLWINDOW              window();
        PHLMONITOR             monitor();

      private:
        WP<CWLSurfaceResource> m_focusSurface;
        PHLWINDOWREF           m_focusWindow;
        PHLMONITORREF          m_focusMonitor;

        CHyprSignalListener    m_windowOpen, m_windowClose;
    };

    SP<CFocusState> focusState();
};
