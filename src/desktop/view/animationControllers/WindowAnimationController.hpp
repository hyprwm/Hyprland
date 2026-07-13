#pragma once

#include "../../../animation/controller/ViewAnimationController.hpp"

namespace Desktop::View {
    class CWindow;

    class CWindowAnimationController : public Animation::IViewAnimationController {
      public:
        // raw ptr: this should only be a member of CWindow, and will be init'd in the ctor.
        CWindowAnimationController(CWindow* parent);
        virtual ~CWindowAnimationController() = default;

        virtual Animation::SViewAnimationContext animateIn() const override;
        virtual Animation::SViewAnimationContext animateOut() const override;

        void                                     apply(const Animation::SViewAnimationContext& ctx) const;

      private:
        CWindow* m_parent;
    };
};