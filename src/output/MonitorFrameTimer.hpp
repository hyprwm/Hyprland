#pragma once
#include "../helpers/time/Time.hpp"
#include <deque>

namespace Monitor {
    // everything after we stop measuring - the commit, the atomic ioctl, the flip itself - plus timer wakeup jitter.
    inline constexpr Time::steady_dur FRAME_SAFETY_MARGIN = std::chrono::microseconds(800);

    // with no pageflip behind the frame event we have no vblank to aim at, so there is no margin to keep - we only
    // want the render to land after the rest of this loop iteration. the timer wheel counts in µs, so this is the
    // smallest delay that still goes through it instead of being picked first.
    inline constexpr Time::steady_dur FRAME_IDLE_DELAY = std::chrono::microseconds(1);

    class CMonitorFrameTimer {
      public:
        CMonitorFrameTimer();
        void             addRenderCost(const Time::steady_tp& start, const Time::steady_tp& end);
        Time::steady_dur estimatedRenderCost();
        bool             hasSamples() const;

      private:
        struct SRenderTimes {
            Time::steady_tp start;
            Time::steady_tp end;
        };

        std::deque<SRenderTimes> m_renderTimes;
    };
}
