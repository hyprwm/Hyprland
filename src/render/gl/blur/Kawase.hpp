#pragma once

#include "Provider.hpp"

class CShader;

namespace Render {
    enum ePreparedFragmentShader : uint8_t;
}

namespace Render::GL {
    class CHyprOpenGLImpl;

    class CDualKawaseBlurProvider : public IGLBlurProvider {
      public:
        explicit CDualKawaseBlurProvider(CHyprOpenGLImpl& impl);

        eBlurType type() const noexcept override;
        bool      isAnimated() const noexcept override;
        bool      requiresLiveBlur() const noexcept override;
        void      expandDamage(CRegion& damage, float multiplier = 1.F) const override;

      protected:
        SP<CGLFramebuffer>              blurGL(SP<CGLFramebuffer> source, float strength, const CRegion& originalDamage, const SBlurContext& context) final;
        virtual ePreparedFragmentShader finishFragment() const noexcept;
        virtual bool                    requiresPreparedInput() const noexcept;
        virtual void                    updateProviderState(const SBlurContext& context, const CRegion& outputDamage);
        virtual void                    setFinishUniforms(WP<CShader> shader, float strength, const SBlurContext& context) const;
        virtual float                   damageRadius() const;

      private:
        CHyprOpenGLImpl& m_impl;
    };

    float dualKawaseDamageRadius(int64_t size, int64_t passes);
}
