#include "../../FloatingAlgorithm.hpp"

namespace Layout {
    class CAlgorithm;
}

namespace Layout::Floating {
    class CDefaultFloatingAlgorithm : public IFloatingAlgorithm {
      public:
        CDefaultFloatingAlgorithm() = default;
        virtual ~CDefaultFloatingAlgorithm() = default;

        virtual void newTarget(SP<ITarget> target);
        virtual void removeTarget(SP<ITarget> target);
    };
};