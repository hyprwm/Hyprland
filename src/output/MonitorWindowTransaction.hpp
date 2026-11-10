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

namespace Monitor {
    class CMonitorWindowTransaction {
      public:
        explicit CMonitorWindowTransaction(PHLMONITOR monitor);
        ~CMonitorWindowTransaction();

        bool queue(PHLWINDOW window, const CBox& box, bool force = false);
        void remove(PHLWINDOW window, bool discard = true);
        void clear();
        void holdFullscreen(PHLWORKSPACE workspace);
        void flush(PHLWINDOW window);
        bool hasPendingPresentation() const;

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
        SP<CEventLoopTimer>                                  m_timeout;
        bool                                                 m_rendering = false;
        bool                                                 m_applying  = false;
        bool                                                 m_timedOut  = false;

        friend class ::CMonitorWindowTransactionTest;
    };
}
