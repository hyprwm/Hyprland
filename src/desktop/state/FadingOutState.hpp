#pragma once

#include "../../SharedDefs.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../DesktopTypes.hpp"

#include <vector>

namespace Desktop {
    class CFadingOutState {
      public:
        CFadingOutState()  = default;
        ~CFadingOutState() = default;

        const std::vector<PHLWINDOWREF>& windows() const;
        const std::vector<PHLLSREF>&     layers() const;

        void                             add(PHLWINDOW w);
        void                             add(PHLLS ls);
        void                             remove(PHLWINDOW w);
        void                             remove(PHLLS ls);
        void                             cleanupForMonitor(const MONITORID& monid);
        void                             removeExpiredLayers();
        void                             clear();

      private:
        std::vector<PHLWINDOWREF> m_windows;
        std::vector<PHLLSREF>     m_layers;
    };

    UP<CFadingOutState>& fadingOutState();
};
