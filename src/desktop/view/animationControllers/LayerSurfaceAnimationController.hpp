#pragma once

#include "../../../animation/controller/ViewAnimationController.hpp"

namespace Desktop::View {
    class CLayerSurface;

    class CLayerSurfaceAnimationController : public Animation::IViewAnimationController {
      public:
        // raw ptr: this should only be a member of CLayerSurface, and will be init'd in the ctor.
        CLayerSurfaceAnimationController(CLayerSurface* parent);
        virtual ~CLayerSurfaceAnimationController() = default;

        virtual Animation::SViewAnimationContext animateIn() const override;
        virtual Animation::SViewAnimationContext animateOut() const override;

        void                                     apply(const Animation::SViewAnimationContext& ctx) const;

      private:
        CLayerSurface* m_parent;
    };
};
