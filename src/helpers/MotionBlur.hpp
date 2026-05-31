#pragma once

#include "time/Time.hpp"
#include "math/Math.hpp"
#include <optional>

namespace MotionBlur {
    CBox extents(const CBox& previous, const CBox& current);

    struct SState {
        CBox previous;
        CBox current;
        int  samples = 1;

        CBox extents() const;
    };

    class CTracker {
      public:
        void                  record(const CBox& previous, const CBox& current);
        void                  reset();

        std::optional<SState> state(int samples, const Vector2D& offset = {}, bool allowStale = false) const;

      private:
        CBox            m_previous;
        CBox            m_current;
        Time::steady_tp m_updatedAt;
        bool            m_valid = false;
    };
};
