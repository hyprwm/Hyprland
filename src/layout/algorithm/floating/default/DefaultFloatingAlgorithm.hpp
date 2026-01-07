#include "../../FloatingAlgorithm.hpp"

namespace Layout {
    class CAlgorithm;
}

namespace Layout::Floating {
    class CDefaultFloatingAlgorithm : public IFloatingAlgorithm {
      public:
        CDefaultFloatingAlgorithm()          = default;
        virtual ~CDefaultFloatingAlgorithm() = default;

        virtual void newTarget(SP<ITarget> target);
        virtual void movedTarget(SP<ITarget> target);
        virtual void removeTarget(SP<ITarget> target);

        virtual void resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        virtual void moveTarget(const Vector2D& Δ, SP<ITarget> target);

        virtual void swapTargets(SP<ITarget> a, SP<ITarget> b);
    };
};