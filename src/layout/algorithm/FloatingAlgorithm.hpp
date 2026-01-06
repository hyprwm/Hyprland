#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "ModeAlgorithm.hpp"

namespace Layout {

    class ITarget;
    class CAlgorithm;

    class IFloatingAlgorithm : public IModeAlgorithm {
      public:
        virtual ~IFloatingAlgorithm() = default;

      protected:
        IFloatingAlgorithm() = default;

        WP<CAlgorithm> m_parent;

        friend class Layout::CAlgorithm;
    };
}