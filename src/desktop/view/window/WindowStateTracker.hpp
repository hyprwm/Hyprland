#pragma once

#include <cstdint>
#include <optional>

namespace Desktop::View {
    class CWindow;

    struct SWindowStateUpdate {
        std::optional<bool> floating;
        std::optional<bool> pinned;
    };

    class CWindowStateEventTracker {
      public:
        explicit CWindowStateEventTracker(CWindow& window);

        void start();
        void stop();
        void scheduleUpdate();

      private:
        struct SWindowStateSnapshot {
            bool floating = false;
            bool pinned   = false;
        };

        SWindowStateSnapshot                snapshot() const;
        void                                emitUpdate(uint64_t sequence);

        CWindow&                            m_window;
        std::optional<SWindowStateSnapshot> m_lastState;
        uint64_t                            m_sequence = 0;
        bool                                m_pending  = false;
    };
}
