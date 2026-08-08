#pragma once

#include "../helpers/time/Time.hpp"

namespace Monitor {
    // this filters out jitter from timestamps coming from software clocks. ktime_get returns a timestamp
    // from when the event was sent, not when the pageflip actually occured. that means the timestamp we see
    // includes any jitter from our own code + eventloop delay, so it fluctuates +-0.02 ms on best case, or more
    // in worse.
    // this is a workaround, not the fix. reconstructing a vblank we never observed can only ever
    // be guessed. the real fix is a hardware clock where the kernel registers a vblank counter where
    // the flip event carries a true hardware timestamp and a usable sequence number.
    class CPresentationTimeFilter {
      public:
        // raw = timestamp we got, period = AQ calculated time between vblanks
        Time::steady_tp filter(const Time::steady_tp& raw, Time::steady_dur period);

      private:
        std::optional<Time::steady_tp> m_previousTimeSample;
    };
}
