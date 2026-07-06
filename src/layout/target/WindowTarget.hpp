#pragma once

#include "Target.hpp"

#include "../../desktop/view/Window.hpp"

namespace Layout {

    class CWindowTarget : public ITarget {
      public:
        static SP<ITarget> create(PHLWINDOW w);
        virtual ~CWindowTarget() = default;

        virtual eTargetType                                         type();

        virtual void                                                setPositionGlobal(const STargetBox& box, uint8_t flags = TARGET_UPDATE_NONE);
        virtual void                                                assignToSpace(const SP<CSpace>& space, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual PHLWINDOW                                           window() const;

        virtual bool                                                floating();
        virtual void                                                setFloating(bool x);
        virtual std::expected<SGeometryRequested, eGeometryFailure> desiredGeometry();
        virtual eFullscreenMode                                     fullscreenMode();
        virtual void                                                setFullscreenMode(eFullscreenMode mode);
        virtual std::optional<Vector2D>                             minSize();
        virtual std::optional<Vector2D>                             maxSize();
        virtual void                                                damageEntire();
        virtual void                                                warpPositionSize();
        virtual void                                                onUpdateSpace();

      private:
        CWindowTarget(PHLWINDOW w);

        Vector2D     clampSizeForDesired(const Vector2D& size) const;

        void         updatePos(uint8_t flags = TARGET_UPDATE_NONE);

        PHLWINDOWREF m_window;
    };
};
