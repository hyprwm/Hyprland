#pragma once

#include "../../../animation/controller/ViewAnimationController.hpp"

namespace Desktop::View {
    class CPopup;

    class CPopupAnimationController : public Animation::IViewAnimationController {
      public:
        // raw ptr: this should only be a member of CPopup, and will be init'd in the ctor.
        CPopupAnimationController(CPopup* parent);
        virtual ~CPopupAnimationController() = default;

        virtual Animation::SViewAnimationContext animateIn() const override;
        virtual Animation::SViewAnimationContext animateOut() const override;

        void                                     apply(const Animation::SViewAnimationContext& ctx) const;

      private:
        CPopup* m_parent;
    };
};
