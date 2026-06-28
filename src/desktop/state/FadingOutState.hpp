#pragma once

#include "Fadeout.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../DesktopTypes.hpp"

#include <vector>

namespace Desktop {
    class CFadingOutState {
      public:
        CFadingOutState()  = default;
        ~CFadingOutState() = default;

        const std::vector<SP<IFadeout>>& fadeouts() const;

        void                             add(SP<IFadeout> fadeout);
        void                             cleanupForMonitor(PHLMONITOR monitor);
        void                             clear();

      private:
        std::vector<SP<IFadeout>> m_fadeouts;
    };

    UP<CFadingOutState>& fadingOutState();
};
