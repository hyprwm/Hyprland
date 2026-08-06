#pragma once
#include "../helpers/time/Time.hpp"
#include <deque>
#include <optional>

namespace Monitor {
    class CMonitorFrameTimer {
      public:
        CMonitorFrameTimer();

        struct SFrameTarget {
            Time::steady_tp  deadline;
            Time::steady_dur target;
        };

        void             addRenderCost(const Time::steady_tp& start, const Time::steady_tp& end);
        Time::steady_dur estimatedRenderCost() const;
        bool             hasSamples() const;

        void             setRefreshPeriod(int refreshNs, float fallbackHz);
        Time::steady_dur refreshPeriod() const;
        bool             hasRefreshPeriod() const;

        // how late we were for the flip we aimed at, or nullopt if we made it
        std::optional<Time::steady_dur> flipMiss(const Time::steady_tp& when, const Time::steady_tp& aimedAt) const;
        // when to render for the flip at earliestFlip.
        SFrameTarget nextTarget(const Time::steady_tp& now, const Time::steady_tp& earliestFlip) const;

      private:
        struct SRenderTimes {
            Time::steady_tp start;
            Time::steady_tp end;
        };

        std::deque<SRenderTimes> m_renderTimes;
        Time::steady_dur         m_refreshPeriod{};
    };
}
