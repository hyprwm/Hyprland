#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/memory/Memory.hpp"

#include <vector>

class CEventLoopTimer;
class CMonitorWindowTransactionTest;

namespace Monitor {
    class CMonitorWindowTransaction {
      public:
        explicit CMonitorWindowTransaction(PHLMONITOR monitor);
        ~CMonitorWindowTransaction();

        bool queue(PHLWINDOW window, const CBox& box);
        void remove(PHLWINDOW window);
        void clear();

        void onPreRender();
        void onPostRender();
        void onWindowCommit(PHLWINDOW window);

      private:
        bool                      onlyMoves() const;
        bool                      ready() const;
        bool                      matchesPendingResize(PHLWINDOW window) const;
        void                      requestApply();
        void                      apply();

        PHLMONITORREF             m_monitor;
        std::vector<PHLWINDOWREF> m_pending;
        SP<CEventLoopTimer>       m_timeout;
        bool                      m_rendering = false;
        bool                      m_applying  = false;
        bool                      m_timedOut  = false;

        friend class ::CMonitorWindowTransactionTest;
    };
}
