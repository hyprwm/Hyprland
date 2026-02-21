#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/math/Direction.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "../LayoutManager.hpp"

#include <expected>
#include <optional>

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
        void                             moveTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt, bool reposition = false);
        void                             removeTarget(SP<ITarget> target);

        void                             swapTargets(SP<ITarget> a, SP<ITarget> b);
        void                             moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent);

        SP<ITarget>                      getNextCandidate(SP<ITarget> old);

        void                             setFloating(SP<ITarget> target, bool floating, bool reposition = false);

        std::expected<void, std::string> layoutMsg(const std::string_view& sv);
        std::optional<Vector2D>          predictSizeForNewTiledTarget();

        void                             recalculate();
        void                             recenter(SP<ITarget> t);

        void                             resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        void                             moveTarget(const Vector2D& Δ, SP<ITarget> target);

        void                             updateFloatingAlgo(UP<IFloatingAlgorithm>&& algo);
        void                             updateTiledAlgo(UP<ITiledAlgorithm>&& algo);

        const UP<ITiledAlgorithm>&       tiledAlgo() const;
        const UP<IFloatingAlgorithm>&    floatingAlgo() const;

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