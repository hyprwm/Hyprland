#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

namespace Layout {

    class ITarget;
    class CAlgorithm;

    class IFloatingAlgorithm {
      public:
        virtual ~IFloatingAlgorithm() = default;

        virtual void newTarget(SP<ITarget> target)    = 0;
        virtual void removeTarget(SP<ITarget> target) = 0;

      protected:
        IFloatingAlgorithm() = default;

        WP<CAlgorithm> m_parent;

        friend class Layout::CAlgorithm;
    };
}