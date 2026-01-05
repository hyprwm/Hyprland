#include "../../TiledAlgorithm.hpp"

namespace Layout {
    class CAlgorithm;
}

namespace Layout::Tiled {
    class CTestTiledAlgorithm : public ITiledAlgorithm {
      public:
        CTestTiledAlgorithm() = default;
        virtual ~CTestTiledAlgorithm() = default;

        virtual void newTarget(SP<ITarget> target);
        virtual void removeTarget(SP<ITarget> target);
    };
};