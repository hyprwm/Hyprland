#include "../../TiledAlgorithm.hpp"

namespace Layout {
    class CAlgorithm;
}

namespace Layout::Tiled {
    class CTestTiledAlgorithm : public ITiledAlgorithm {
      public:
        CTestTiledAlgorithm()          = default;
        virtual ~CTestTiledAlgorithm() = default;

        virtual void newTarget(SP<ITarget> target);
        virtual void movedTarget(SP<ITarget> target);
        virtual void removeTarget(SP<ITarget> target);

        virtual void resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        virtual void moveTarget(const Vector2D& Δ, SP<ITarget> target);
    };
};