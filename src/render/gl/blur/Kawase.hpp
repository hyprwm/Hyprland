#pragma once

#include "Material.hpp"
#include "Provider.hpp"

namespace Render::GL {
    class CHyprOpenGLImpl;

    class CDualKawaseBlurProvider : public IGLBlurProvider {
      public:
        explicit CDualKawaseBlurProvider(CHyprOpenGLImpl& impl);
        CDualKawaseBlurProvider(CHyprOpenGLImpl& impl, UP<IGLBlurMaterial> material);

        eBlurType type() const noexcept override;
        bool      isAnimated(const CRenderingContext& context) const noexcept override;
        bool      requiresLiveBlur() const noexcept override;
        void      expandDamage(CRegion& damage, float multiplier = 1.F) const override;

      protected:
        SP<CGLFramebuffer> blurGL(CRenderingContext& renderingContext, SP<CGLFramebuffer> source, float strength, const CRegion& originalDamage,
                                  const SBlurContext& blurContext) override;

      private:
        float               damageRadius() const;

        CHyprOpenGLImpl&    m_impl;
        UP<IGLBlurMaterial> m_material;
    };

    float dualKawaseDamageRadius(int64_t size, int64_t passes);
}
