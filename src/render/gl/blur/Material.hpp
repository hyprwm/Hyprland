#pragma once

#include "../../ShaderLoader.hpp"
#include "../../blur/Provider.hpp"

class CShader;

namespace Render::GL {
    class CHyprOpenGLImpl;

    struct SBlurMaterialRequirements {
        ePreparedFragmentShader finishFragment = SH_FRAG_BLURFINISH;
        bool                    preparedInput  = false;
        bool                    liveBlur       = false;
    };

    struct SBlurMaterialContext {
        const SBlurContext& blurContext;
        const CRegion&      outputDamage;
        float               strength = 1.F;
    };

    class IGLBlurMaterial {
      public:
        virtual ~IGLBlurMaterial() = default;

        virtual eBlurType                 type() const noexcept         = 0;
        virtual SBlurMaterialRequirements requirements() const noexcept = 0;
        virtual bool                      isAnimated() const noexcept;
        virtual int64_t                   blurSizeForDamage(int64_t size) const;
        virtual float                     sampleRadius() const;
        virtual void                      prepare(const SBlurMaterialContext& context);
        virtual void                      bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const;

      protected:
        IGLBlurMaterial() = default;
    };

    class CDefaultBlurMaterial final : public IGLBlurMaterial {
      public:
        eBlurType                 type() const noexcept override;
        SBlurMaterialRequirements requirements() const noexcept override;
        int64_t                   blurSizeForDamage(int64_t size) const override;
    };
}
