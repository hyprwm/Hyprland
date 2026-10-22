#pragma once

#include "Fadeout.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../DesktopTypes.hpp"

#include <vector>

namespace Desktop {
    class CFadingOutState {
      public:
        CFadingOutState();
        ~CFadingOutState() = default;

        CFadingOutState(const CFadingOutState&)                            = delete;
        CFadingOutState&                 operator=(const CFadingOutState&) = delete;

        const std::vector<SP<IFadeout>>& fadeouts() const;

        void                             add(SP<IFadeout> fadeout);
        void                             cleanupForMonitor(PHLMONITOR monitor);
        void                             clear();

      private:
        void                      cleanup(PHLMONITOR monitor = nullptr);

        std::vector<SP<IFadeout>> m_fadeouts;
        CHyprSignalListener       m_tickListener;
    };

    UP<CFadingOutState>& fadingOutState();
};
