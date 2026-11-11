#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/memory/Memory.hpp"

#include <vector>

class CEventLoopTimer;
class CMonitorWindowTransactionTest;

namespace Fullscreen {
    struct SFullscreenPresentation;
}

namespace Desktop {
    class CWindowFadeout;
}

namespace Monitor {
    class CMonitorWindowTransaction {
      public:
        explicit CMonitorWindowTransaction(PHLMONITOR monitor);
        ~CMonitorWindowTransaction();

        bool queue(PHLWINDOW window, const CBox& box, bool force = false);
        void remove(PHLWINDOW window, bool discard = true);
        void clear();
        void holdFullscreen(PHLWORKSPACE workspace);
        bool holdClosingWindow(SP<Desktop::CWindowFadeout> fadeout);
        void flush(PHLWINDOW window);
        bool hasPendingChanges() const;
        bool hasPendingPresentation() const;
        void beginLayoutUpdate();
        void endLayoutUpdate();

        void onPreRender();
        void onPostRender();
        void onWindowCommit(PHLWINDOW window);

      private:
        bool                                                 onlyMoves() const;
        bool                                                 ready() const;
        bool                                                 matchesPendingResize(PHLWINDOW window) const;
        void                                                 requestApply();
        void                                                 apply();

        PHLMONITORREF                                        m_monitor;
        std::vector<PHLWINDOWREF>                            m_pending;
        std::vector<SP<Fullscreen::SFullscreenPresentation>> m_fullscreen;
        std::vector<SP<Desktop::CWindowFadeout>>             m_closingWindows;
        SP<CEventLoopTimer>                                  m_timeout;
        bool                                                 m_rendering         = false;
        bool                                                 m_applying          = false;
        bool                                                 m_timedOut          = false;
        size_t                                               m_layoutUpdateDepth = 0;

        friend class ::CMonitorWindowTransactionTest;
    };
}
