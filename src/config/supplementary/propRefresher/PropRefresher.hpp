#pragma once

#include <cstdint>
#include <type_traits>

#include "../../../helpers/memory/Memory.hpp"

namespace Config::Supplementary {
    enum ePropRefreshProp : uint16_t {
        REFRESH_LAYOUTS            = (1 << 0),
        REFRESH_INPUT_DEVICES      = (1 << 1),
        REFRESH_SCREEN_SHADER      = (1 << 2),
        REFRESH_BLUR_FB            = (1 << 3),
        REFRESH_RULES              = (1 << 4),
        REFRESH_WINDOW_STATES      = (1 << 5) | REFRESH_RULES,
        REFRESH_MONITOR_STATES     = (1 << 6) | REFRESH_LAYOUTS,
        REFRESH_CURSOR_ZOOMS       = (1 << 7),
        REFRESH_CONFIG_WATCHER     = (1 << 8),
        REFRESH_GRADIENTS_GROUPBAR = (1 << 9),

        REFRESH_ALL = std::numeric_limits<std::underlying_type_t<ePropRefreshProp>>::max(),
    };

    using PropRefreshBits = std::underlying_type_t<ePropRefreshProp>;

    class CPropRefresher {
      public:
        void scheduleRefresh(PropRefreshBits reason);
        int  executeScheduledRefreshImmediately();

      private:
        void            refreshProp(const bool execdAsScheduled);

        bool            m_scheduled           = false;
        uint64_t        m_scheduledRefreshSeq = 0; // 0 if no refresh event scheduled
        PropRefreshBits m_propsTripped        = 0;
    };

    UP<CPropRefresher>& refresher();
};
