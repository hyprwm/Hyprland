#pragma once

#include "../helpers/memory/Memory.hpp"

namespace State {
    class CMonitorLayoutController {
      public:
        void scheduleRecheck();
        void arrange() const;
        void checkOverlapsAndNotify() const;

      private:
        bool m_scheduled = false;
    };

    UP<CMonitorLayoutController>& monitorLayoutController();
}
