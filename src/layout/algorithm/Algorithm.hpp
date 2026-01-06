#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "../LayoutManager.hpp"

#include <expected>

namespace Layout {
    class ITarget;
    class IFloatingAlgorithm;
    class ITiledAlgorithm;
    class CSpace;

    class CAlgorithm {
      public:
        static SP<CAlgorithm> create(UP<ITiledAlgorithm>&& tiled, UP<IFloatingAlgorithm>&& floating, SP<CSpace> space);
        ~CAlgorithm() = default;

        void                             addTarget(SP<ITarget> target);
        void                             moveTarget(SP<ITarget> target);
        void                             removeTarget(SP<ITarget> target);

        void                             setFloating(SP<ITarget> target, bool floating);

        std::expected<void, std::string> layoutMsg(const std::string_view& sv);

        void                             recalculate();

        void                             resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        void                             moveTarget(const Vector2D& Δ, SP<ITarget> target);

        SP<CSpace>                       space() const;

        size_t                           tiledTargets() const;
        size_t                           floatingTargets() const;

      private:
        CAlgorithm(UP<ITiledAlgorithm>&& tiled, UP<IFloatingAlgorithm>&& floating, SP<CSpace> space);

        UP<ITiledAlgorithm>      m_tiled;
        UP<IFloatingAlgorithm>   m_floating;
        WP<CSpace>               m_space;
        WP<CAlgorithm>           m_self;

        std::vector<WP<ITarget>> m_tiledTargets, m_floatingTargets;
    };
}