#pragma once

#include <cstdint>
#include <type_traits>

#include "../../../helpers/memory/Memory.hpp"

namespace Config::Supplementary {
    enum ePropRefreshProp : uint8_t {
        REFRESH_LAYOUTS        = (1 << 0),
        REFRESH_INPUT_DEVICES  = (1 << 1),
        REFRESH_SCREEN_SHADER  = (1 << 2),
        REFRESH_BLUR_FB        = (1 << 3),
        REFRESH_RULES          = (1 << 4),
        REFRESH_WINDOW_STATES  = (1 << 5) | REFRESH_RULES,
        REFRESH_MONITOR_STATES = (1 << 6) | REFRESH_LAYOUTS,
    };

    using PropRefreshBits = std::underlying_type_t<ePropRefreshProp>;

    class CPropRefresher {
      public:
        void scheduleRefresh(PropRefreshBits reason);

      private:
        bool            m_scheduled = false;
        PropRefreshBits m_propsTripped;
    };

    UP<CPropRefresher>& refresher();
};