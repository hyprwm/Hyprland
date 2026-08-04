#pragma once

#include "Kawase.hpp"

namespace Render::GL {
    class CHazeBlurMaterial final : public IGLBlurMaterial {
      public:
        eBlurType                 type() const noexcept override;
        SBlurMaterialRequirements requirements() const noexcept override;
        int64_t                   blurSizeForDamage(int64_t size) const override;
        void                      bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const override;
    };

    class CHazeBlurProvider final : public CDualKawaseBlurProvider {
      public:
        explicit CHazeBlurProvider(CHyprOpenGLImpl& impl);
    };
}
