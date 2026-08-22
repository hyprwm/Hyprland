#pragma once

#include "Kawase.hpp"

namespace Render::GL {
    class CGlassBlurMaterial : public IGLBlurMaterial {
      public:
        CGlassBlurMaterial(eBlurType type, ePreparedFragmentShader finishFragment, bool preparedInput = false);

        eBlurType                 type() const noexcept override;
        SBlurMaterialRequirements requirements() const noexcept override;
        float                     sampleRadius() const override;
        void                      bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const override;

      private:
        const eBlurType               m_type;
        const ePreparedFragmentShader m_finishFragment;
        const bool                    m_preparedInput;
    };

    class CGlassBlurProvider : public CDualKawaseBlurProvider {
      public:
        CGlassBlurProvider(CHyprOpenGLImpl& impl, eBlurType type, ePreparedFragmentShader finishFragment);

      protected:
        CGlassBlurProvider(CHyprOpenGLImpl& impl, UP<IGLBlurMaterial> material);
    };

    float glassDamageRadius(int64_t size, int64_t passes, float refraction);
}
