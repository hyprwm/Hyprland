#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/math/Direction.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "../../desktop/DesktopTypes.hpp"

#include "../LayoutManager.hpp"

#include <optional>
#include <expected>

namespace Layout {
    class ITarget;
    class CAlgorithm;

    class CSpace {
      public:
        static SP<CSpace> create(PHLWORKSPACE w);
        ~CSpace() = default;

        void                             add(SP<ITarget> t);
        void                             remove(SP<ITarget> t);
        void                             move(SP<ITarget> t, std::optional<Vector2D> focalPoint = std::nullopt);

        void                             swap(SP<ITarget> a, SP<ITarget> b);

        SP<ITarget>                      getNextCandidate(SP<ITarget> old);

        void                             setAlgorithmProvider(SP<CAlgorithm> algo);
        void                             recheckWorkArea();
        void                             setFullscreen(SP<ITarget> t, eFullscreenMode mode);

        void                             moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent);

        void                             recalculate();

        void                             toggleTargetFloating(SP<ITarget> t);

        std::expected<void, std::string> layoutMsg(const std::string_view& sv);
        std::optional<Vector2D>          predictSizeForNewTiledTarget();

        const CBox&                      workArea(bool floating = false) const;
        PHLWORKSPACE                     workspace() const;
        CBox                             targetPositionLocal(SP<ITarget> t) const;

        void                             resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        void                             moveTarget(const Vector2D& Δ, SP<ITarget> target);

        SP<CAlgorithm>                   algorithm() const;

        const std::vector<WP<ITarget>>&  targets() const;

      private:
        CSpace(PHLWORKSPACE parent);

        WP<CSpace>               m_self;

        std::vector<WP<ITarget>> m_targets;
        SP<CAlgorithm>           m_algorithm;
        PHLWORKSPACEREF          m_parent;

        // work area is in global coords
        CBox m_workArea, m_floatingWorkArea;
    };
};