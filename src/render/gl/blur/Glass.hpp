#pragma once

#include "Kawase.hpp"

namespace Render::GL {
    class CGlassBlurProvider : public CDualKawaseBlurProvider {
      public:
        CGlassBlurProvider(CHyprOpenGLImpl& impl, eBlurType type, ePreparedFragmentShader finishFragment);

        eBlurType type() const noexcept override;

      protected:
        ePreparedFragmentShader finishFragment() const noexcept override;
        void                    setFinishUniforms(WP<CShader> shader, float strength, const SBlurContext& context) const override;
        float                   damageRadius() const override;

      private:
        const eBlurType               m_type;
        const ePreparedFragmentShader m_finishFragment;
    };

    float glassDamageRadius(int64_t size, int64_t passes, float refraction);
}
