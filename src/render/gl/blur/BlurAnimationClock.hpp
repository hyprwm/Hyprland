#pragma once

#include "../../../helpers/time/Time.hpp"

namespace Render::GL {
    class CBlurAnimationClock {
      public:
        explicit CBlurAnimationClock(Time::steady_tp now = Time::steadyNow());

        // Read-only capture sampling never moves the live anchor or changes its speed.
        double sample(Time::steady_tp now) const;
        double update(Time::steady_tp now, float speed);

      private:
        Time::steady_tp m_lastUpdate;
        double          m_time          = 0.0;
        float           m_previousSpeed = 0.F;
    };
}
