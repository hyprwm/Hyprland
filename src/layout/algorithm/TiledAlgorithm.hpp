#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "ModeAlgorithm.hpp"

namespace Layout {

    class ITarget;
    class CAlgorithm;

    class ITiledAlgorithm : public IModeAlgorithm {
      public:
        virtual ~ITiledAlgorithm() = default;

      protected:
        ITiledAlgorithm() = default;

        WP<CAlgorithm> m_parent;

        friend class Layout::CAlgorithm;
    };
}