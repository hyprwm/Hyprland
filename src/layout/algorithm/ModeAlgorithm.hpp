#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/math/Direction.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "../LayoutManager.hpp"
#include "../space/Space.hpp"

namespace Fullscreen {
    class IFullscreenHandler;
}

namespace Layout {

    class ITarget;
    class CAlgorithm;

    class IModeAlgorithm {
      public:
        IModeAlgorithm();
        virtual ~IModeAlgorithm() = default;

        // a completely new target
        virtual void newTarget(SP<ITarget> target) = 0;

        // a target moved into the algorithm (from another)
        virtual void movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt) = 0;

        // a target removed
        virtual void removeTarget(SP<ITarget> target) = 0;

        // a target is being resized by a delta. Corner none likely means not interactive
        virtual void resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE) = 0;

        // recalculate layout
        virtual void recalculate(eRecalculateReason reason = RECALCULATE_REASON_UNKNOWN) = 0;

        // swap targets
        virtual void swapTargets(SP<ITarget> a, SP<ITarget> b) = 0;

        // move a target in a given direction
        virtual void moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) = 0;

        // optional: handle layout messages
        virtual Config::ErrorResult layoutMsg(const std::string_view& sv);

        // optional: predict new window's size
        virtual std::optional<Vector2D> predictSizeForNewTarget();

        // optional: allow algorithms to own fullscreen semantics for a target.
        // virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);

        // optional: expose an algorithm-owned fullscreen target

        /**
        * Get the current layout managed FS target
        * @return Covers the monitor(`FSMODE_FULLSCREEN`) / work area(`FSMODE_MAXIMIZE`)
        */
        // virtual SP<ITarget> layoutFullscreenTarget() const;

        // optional: allow layouts to own layer/window hiding logic for fullscreen targets

        // virtual void setNoMembersAboveFullscreen();

        // Impl'd here: focal point for dir
        virtual std::optional<Vector2D> focalPointForDir(SP<ITarget> t, Math::eDirection dir);

      protected:
        IModeAlgorithm();

        WP<CAlgorithm> m_parent;
        // Layouts that wish to implement custom FS handlers must overwrite this in their constructors
        const UP<Fullscreen::IFullscreenHandler> m_defaultFullscreenHandler;

        friend class Layout::CAlgorithm;
        friend class Fullscreen::IFullscreenHandler;
    };
}
