#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "ModeAlgorithm.hpp"

#include <optional>
#include <string>

namespace Layout {

    class ITarget;
    class CAlgorithm;

    class ITiledAlgorithm : public IModeAlgorithm {
      public:
        virtual ~ITiledAlgorithm() = default;

        virtual SP<ITarget> getNextCandidate(SP<ITarget> old) = 0;

        // Optional runtime layout name. Useful for generic adapter classes where
        // typeid alone cannot identify the selected layout instance.
        virtual std::optional<std::string> layoutName() const {
            return std::nullopt;
        }

      protected:
        ITiledAlgorithm() = default;

        WP<CAlgorithm> m_parent;

        friend class Layout::CAlgorithm;
    };
}
