#pragma once

#include "../../helpers/AnimatedVariable.hpp"

#include <optional>

namespace Fullscreen {
    struct SFullscreenPresentation;
}

namespace Workspace {
    class CWorkspacePresentable {
      public:
        CWorkspacePresentable()  = default;
        ~CWorkspacePresentable() = default;

        CWorkspacePresentable(const CWorkspacePresentable&) = delete;
        CWorkspacePresentable(CWorkspacePresentable&)       = delete;
        CWorkspacePresentable(CWorkspacePresentable&&)      = delete;

        PHLANIMVAR<Vector2D>                    m_renderOffset;
        PHLANIMVAR<float>                       m_alpha;

        std::optional<std::string>              m_animationStyle;

        WP<Fullscreen::SFullscreenPresentation> m_fullscreenPresentation;
    };
}
