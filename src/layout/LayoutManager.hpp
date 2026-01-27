#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../helpers/math/Math.hpp"
#include "../managers/input/InputManager.hpp"

#include "supplementary/DragController.hpp"

#include <optional>
#include <expected>

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

    class CLayoutManager {
      public:
        CLayoutManager();
        ~CLayoutManager() = default;

        void                             newTarget(SP<ITarget> target, SP<CSpace> space);
        void                             removeTarget(SP<ITarget> target);

        void                             changeFloatingMode(SP<ITarget> target);

        void                             beginDragTarget(SP<ITarget> target, eMouseBindMode mode);
        void                             moveMouse(const Vector2D& mousePos);
        void                             resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        void                             moveTarget(const Vector2D& Δ, SP<ITarget> target);
        void                             endDragTarget();

        std::expected<void, std::string> layoutMsg(const std::string_view& sv);

        void                             fullscreenRequestForTarget(SP<ITarget> target, eFullscreenMode currentEffectiveMode, eFullscreenMode effectiveMode);

        void                             switchTargets(SP<ITarget> a, SP<ITarget> b, bool preserveFocus = true);

        void                             moveInDirection(SP<ITarget> target, const std::string& direction, bool silent = false);

        SP<ITarget>                      getNextCandidate(SP<CSpace> space, SP<ITarget> from);

        bool                             isReachable(SP<ITarget> target);

        void                             bringTargetToTop(SP<ITarget> target);

        std::optional<Vector2D>          predictSizeForNewTiledTarget();

        void                             performSnap(Vector2D& sourcePos, Vector2D& sourceSize, SP<ITarget> target, eMouseBindMode mode, int corner, const Vector2D& beginSize);

        void                             invalidateMonitorGeometries(PHLMONITOR);
        void                             recalculateMonitor(PHLMONITOR);

        const UP<Supplementary::CDragStateController>& dragController();

      private:
        UP<Supplementary::CDragStateController> m_dragStateController = makeUnique<Supplementary::CDragStateController>();
    };
}

inline UP<Layout::CLayoutManager> g_layoutManager;