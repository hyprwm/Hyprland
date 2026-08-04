#pragma once

#include "Kawase.hpp"

namespace Render::GL {
    class CAcrylicBlurMaterial final : public IGLBlurMaterial {
      public:
        eBlurType                 type() const noexcept override;
        SBlurMaterialRequirements requirements() const noexcept override;
        float                     sampleRadius() const override;
        void                      bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const override;
    };

    class CAcrylicBlurProvider final : public CDualKawaseBlurProvider {
      public:
        explicit CAcrylicBlurProvider(CHyprOpenGLImpl& impl);
    };

    float acrylicDamageRadius(int64_t size, int64_t passes, float refraction);
}
