#pragma once

#include "../../helpers/AnimatedVariable.hpp"

#include <optional>

namespace Workspace {
    class CWorkspacePresentable {
      public:
        CWorkspacePresentable()  = default;
        ~CWorkspacePresentable() = default;

        CWorkspacePresentable(const CWorkspacePresentable&) = delete;
        CWorkspacePresentable(CWorkspacePresentable&)       = delete;
        CWorkspacePresentable(CWorkspacePresentable&&)      = delete;

        PHLANIMVAR<Vector2D>       m_renderOffset;
        PHLANIMVAR<float>          m_alpha;

        std::optional<std::string> m_animationStyle;
    };
}
