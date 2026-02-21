#pragma once

#include "Target.hpp"

#include "../../desktop/view/Window.hpp"
#include "../../desktop/view/Group.hpp"

namespace Layout {

    class CWindowGroupTarget : public ITarget {
      public:
        static SP<CWindowGroupTarget> create(SP<Desktop::View::CGroup> g);
        virtual ~CWindowGroupTarget() = default;

        virtual eTargetType                                         type();

        virtual void                                                setPositionGlobal(const CBox& box);
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
        CWindowGroupTarget(SP<Desktop::View::CGroup> g);

        void                      updatePos();

        WP<Desktop::View::CGroup> m_group;
    };
};