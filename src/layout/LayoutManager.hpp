#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../helpers/math/Math.hpp"

#include <optional>

enum eFullscreenMode : int8_t;

namespace Layout {
    class ITarget;
    class CSpace;

    enum eRectCorner : uint8_t {
        CORNER_NONE        = 0,
        CORNER_TOPLEFT     = (1 << 0),
        CORNER_TOPRIGHT    = (1 << 1),
        CORNER_BOTTOMRIGHT = (1 << 2),
        CORNER_BOTTOMLEFT  = (1 << 3),
    };

    inline eRectCorner cornerFromBox(const CBox& box, const Vector2D& pos) {
        const auto CENTER = box.middle();

        if (pos.x < CENTER.x)
            return pos.y < CENTER.y ? CORNER_TOPLEFT : CORNER_BOTTOMLEFT;
        return pos.y < CENTER.y ? CORNER_TOPRIGHT : CORNER_BOTTOMRIGHT;
    }

    enum eSnapEdge : uint8_t {
        SNAP_INVALID = 0,
        SNAP_UP      = (1 << 0),
        SNAP_DOWN    = (1 << 1),
        SNAP_LEFT    = (1 << 2),
        SNAP_RIGHT   = (1 << 3),
    };

    enum eDirection : int8_t {
        DIRECTION_DEFAULT = -1,
        DIRECTION_UP      = 0,
        DIRECTION_RIGHT,
        DIRECTION_DOWN,
        DIRECTION_LEFT
    };

    class CLayoutManager {
      public:
        CLayoutManager()  = default;
        ~CLayoutManager() = default;

        void                    newTarget(SP<ITarget> target, SP<CSpace> space);
        void                    removeTarget(SP<ITarget> target);

        void                    changeFloatingMode(SP<ITarget> target);

        void                    beginDragTarget(SP<ITarget> target);
        void                    resizeTarget(SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        void                    moveTarget(SP<ITarget> target);
        void                    endDragTarget(SP<ITarget> target);

        void                    fullscreenRequestForTarget(SP<ITarget> target, eFullscreenMode currentEffectiveMode, eFullscreenMode effectiveMode);

        void                    switchTargets(SP<ITarget> a, SP<ITarget> b);

        void                    moveInDirection(SP<ITarget> target, const std::string& direction, bool silent = false);

        SP<ITarget>             getNextCandidate(SP<ITarget> from);

        bool                    isReachable(SP<ITarget> target);

        void                    bringTargetToTop(SP<ITarget> target);

        std::optional<Vector2D> predictSizeForNewTiledTarget();

        void                    fitIfFloatingOnMonitor(SP<ITarget> target);
    };
}

inline UP<Layout::CLayoutManager> g_layoutManager;